#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <vector>

struct ProgramState;
struct Interpreter;
struct Runner;

struct RawFunction {
	const char *name;
	void (*run)(ProgramState&);
	void (*ignore)(Runner&) = nullptr;
};

using word_idx_t = uint64_t;
static_assert(
	sizeof(word_idx_t) == sizeof(std::size_t),
	"Expected word size to be 64 bits"
);
using primitive_idx_t = uint64_t;
static_assert(
	sizeof(primitive_idx_t) == sizeof(std::size_t),
	"Expected word size to be 64 bits"
);
union number_t {
	uint64_t pos;
	int64_t sign;
};
static_assert(
	sizeof(number_t) == sizeof(std::size_t),
	"Expected word size to be 64 bits"
);
using function_ptr_t = RawFunction *;
static_assert(
	sizeof(function_ptr_t) == sizeof(std::size_t),
	"Expected word size to be 64 bits"
);

struct Value {
	enum Type {
		Word,
		Primative,
		Number,
		RawFunction,
	} type;
	union {
		uint64_t raw_value;
		word_idx_t word_idx;
		primitive_idx_t primitive_idx;
		number_t number;
		function_ptr_t function_ptr;
	};
};

void run_word_idx(word_idx_t word_idx, ProgramState &state);
void run_number(number_t number, ProgramState &state);
void run_function_ptr(function_ptr_t function_ptr, ProgramState &state);

using Stack = std::vector<number_t>;

struct Primitive {
	const char *name;
	const char *desc;
	union {
		void (*fun)(ProgramState&); // simple function
		struct { // complex function
			void (*run)(Runner&);
			void (*interpret)(Interpreter&);
		};
	};
	bool complex = false;
	std::optional<size_t> (*compile)(Interpreter&) = nullptr;
	void (*ignore)(Interpreter&) = nullptr;
};

struct Word {
	const char *name;
	const char *desc;
	word_idx_t code_pos;
	size_t code_len;
};

struct ProgramState {
	Stack stack;
	std::vector<Value> code;
	const char *error;

	std::vector<Word> words;
	Primitive *primitives;
	std::size_t primitives_len;
};

struct Interpreter {
	const char *line = nullptr;
	std::size_t len = 0;
	enum {
		Run,
		Compile,
		Ignore,
	} action;
	struct {
		const char *text;
		std::size_t len;
	} curr_word;
	ProgramState &state;

	void get_word() {
		while (len > 0 && line[0] == ' ') {
			--len;
			++line;
		}
		curr_word.text = line;
		while (len > 0 && line[0] != ' ') {
			--len;
			++line;
		}
		curr_word.len = line - curr_word.text;
	}
	void unget_word() {
		line = curr_word.text;
		len += curr_word.len;

		curr_word.text = nullptr;
		curr_word.len = 0;
	}

	std::optional<word_idx_t> read_word_idx() {
		get_word();

		if (curr_word.len == 0) return {};

		word_idx_t i = state.words.size();
		while (i --> 0) {
			if (strlen(state.words[i].name) != curr_word.len) continue;

			if (strncmp(state.words[i].name, curr_word.text, curr_word.len) == 0) {
				return i;
			}
		}

		unget_word();
		return {};
	}
	std::optional<primitive_idx_t> read_primitive_idx() {
		get_word();

		if (curr_word.len == 0) return {};

		primitive_idx_t i = state.primitives_len;
		while (i --> 0) {
			if (strlen(state.primitives[i].name) != curr_word.len) continue;

			if (strncmp(state.primitives[i].name, curr_word.text, curr_word.len) == 0) {
				return i;
			}
		}

		unget_word();
		return {};
	}
	std::optional<number_t> read_number() {
		get_word();

		number_t number = {0};
		for (size_t i = 0; i < curr_word.len; ++i) {
			if (curr_word.text[i] < '0' || curr_word.text[i] > '9') {
				unget_word();
				return {};
			}

			const number_t prev = number;

			number.pos *= 10;
			number.pos += curr_word.text[i] - '0';

			if (number.pos < prev.pos) {
				// overflow detected
				state.error = "Error: Number number too large!";
				unget_word();
				return {};
			}
		}

		return number;
	}
	std::optional<Value> read_value() {
		const auto word_idx = read_word_idx();
		if (word_idx.has_value()) return { {
			.type = Value::Word,
			.word_idx = word_idx.value(),
		} };

		const auto primitive_idx = read_primitive_idx();
		if (primitive_idx.has_value()) return { {
			.type = Value::Primative,
			.primitive_idx = primitive_idx.value(),
		} };

		const auto number = read_number();
		if (number.has_value()) return { {
			.type = Value::Number,
			.number = number.value(),
		} };

		state.error = "Error: undefined word";

		return {};
	}

	void run_word_idx(word_idx_t word_idx);
	void run_primitive_idx(primitive_idx_t primitive_idx);
	void run_number(number_t number);
	void run_value(Value value) {
		switch (value.type) {
			case Value::Word: {
				run_word_idx(value.word_idx);
			} break;
			case Value::Primative: {
				run_primitive_idx(value.primitive_idx);
			} break;
			case Value::Number: {
				run_number(value.number);
			} break;
			case Value::RawFunction: {
				state.error = "Error: cannot interpret raw function";
			} break;
		}
	}

	bool run_next();

	std::optional<std::size_t> compile_word_idx(word_idx_t word_idx);
	std::optional<std::size_t> compile_primitive_idx(primitive_idx_t primitive_idx);
	std::optional<std::size_t> compile_number(number_t number);
	std::optional<std::size_t> compile_value(Value value) {
		switch (value.type) {
			case Value::Word: {
				return compile_word_idx(value.word_idx);
			} break;
			case Value::Primative: {
				return compile_primitive_idx(value.primitive_idx);
			} break;
			case Value::Number: {
				return compile_number(value.number);
			} break;
			case Value::RawFunction: {
				state.error = "Error: cannot interpret raw function";
				return {};
			} break;
		}
	}

	std::optional<std::size_t> compile_next();

	void ignore_word_idx(word_idx_t word_idx);
	void ignore_primitive_idx(primitive_idx_t primitive_idx);
	void ignore_number(number_t number);
	void ignore_value(Value value) {
		switch (value.type) {
			case Value::Word: {
				ignore_word_idx(value.word_idx);
			} break;
			case Value::Primative: {
				ignore_primitive_idx(value.primitive_idx);
			} break;
			case Value::Number: {
				ignore_number(value.number);
			} break;
			case Value::RawFunction: {
				state.error = "Error: cannot interpret raw function";
			} break;
		}
	}

	bool ignore_next();

	bool advance() {
		switch (action) {
			case Run: return run_next();
			case Compile: return compile_next().has_value();
			case Ignore: return ignore_next();
		}
	}
};

struct Runner {
	Value *code = nullptr;
	std::size_t len = 0;
	enum {
		Run,
		Ignore,
	} action;
	ProgramState &state;

	std::optional<Value> read_value() {
		if (len == 0) return {};
		const auto res = *code;
		++code;
		--len;
		return res;
	}

	void run_word_idx(word_idx_t word_idx);
	void run_primitive_idx(primitive_idx_t primitive_idx);
	void run_number(number_t number);
	void run_function_ptr(function_ptr_t function_ptr);
	void run_value(Value value) {
		switch (value.type) {
			case Value::Word: {
				run_word_idx(value.word_idx);
			} break;
			case Value::Primative: {
				run_primitive_idx(value.primitive_idx);
			} break;
			case Value::Number: {
				run_number(value.number);
			} break;
			case Value::RawFunction: {
				run_function_ptr(value.function_ptr);
			} break;
		}
	}

	bool run_next();

	void ignore_word_idx(word_idx_t word_idx);
	void ignore_primitive_idx(primitive_idx_t primitive_idx);
	void ignore_number(number_t number);
	void ignore_function_ptr(function_ptr_t function_ptr);
	void ignore_value(Value value) {
		switch (value.type) {
			case Value::Word: {
				ignore_word_idx(value.word_idx);
			} break;
			case Value::Primative: {
				ignore_primitive_idx(value.primitive_idx);
			} break;
			case Value::Number: {
				ignore_number(value.number);
			} break;
			case Value::RawFunction: {
				ignore_function_ptr(value.function_ptr);
			} break;
		}
	}

	bool ignore_next();

	bool advance() {
		switch (action) {
			case Run: return run_next();
			case Ignore: return ignore_next();
		}
	}
};

void run_word_idx(word_idx_t word_idx, ProgramState &state) {
	assert(word_idx < state.words.size());

	const auto &word = state.words[word_idx];
	assert(word.code_pos <= state.code.size());
	assert(word.code_pos + word.code_len <= state.code.size());

	Runner runner = {
		.code = &state.code[word.code_pos],
		.len = word.code_len,
		.action = Runner::Run,
		.state = state,
	};

	while (!state.error && runner.len > 0) {
		runner.advance();
	}
}
void run_number(number_t number, ProgramState &state) {
	state.stack.push_back(number);
}
void run_function_ptr(function_ptr_t function_ptr, ProgramState &state) {
	function_ptr->run(state);
}

void Interpreter::run_word_idx(word_idx_t word_idx) {
	::run_word_idx(word_idx, state);
}
void Interpreter::run_primitive_idx(primitive_idx_t primitive_idx) {
	assert(primitive_idx < state.primitives_len);

	const auto &primitive = state.primitives[primitive_idx];
	if (primitive.complex) {
		primitive.interpret(*this);
	} else {
		primitive.fun(state);
	}
}
void Interpreter::run_number(number_t number) {
	::run_number(number, state);
}

bool Interpreter::run_next() {
	const auto value = read_value();
	if (value.has_value()) {
		run_value(value.value());
		return true;
	} else return false;
}

std::optional<std::size_t> Interpreter::compile_word_idx(word_idx_t word_idx) {
	assert(word_idx < state.words.size());

	state.code.push_back({ .type = Value::Word, .word_idx = word_idx });
	return 1;
}
std::optional<std::size_t> Interpreter::compile_primitive_idx(primitive_idx_t primitive_idx) {
	assert(primitive_idx < state.primitives_len);

	const auto &primitive = state.primitives[primitive_idx];

	if (primitive.compile != nullptr) {
		return primitive.compile(*this);
	} else {
		state.code.push_back({
			.type = Value::Primative,
			.primitive_idx = primitive_idx,
		});
		return 1;
	}
}
std::optional<std::size_t> Interpreter::compile_number(number_t number) {
	state.code.push_back({ .type = Value::Number, .number = number });
	return 1;
}

std::optional<std::size_t> Interpreter::compile_next() {
	const auto value = read_value();
	if (value.has_value()) {
		return compile_value(value.value());
	}
	return {};
}

void Interpreter::ignore_word_idx(word_idx_t) { }
void Interpreter::ignore_primitive_idx(primitive_idx_t primitive_idx) {
	assert(primitive_idx < state.primitives_len);

	const auto &primitive = state.primitives[primitive_idx];

	if (primitive.ignore != nullptr) {
		return primitive.ignore(*this);
	}
}
void Interpreter::ignore_number(number_t) { }

bool Interpreter::ignore_next() {
	const auto value = read_value();
	if (value.has_value()) {
		ignore_value(value.value());
		return true;
	} return false;
}

void Runner::run_word_idx(word_idx_t word_idx) {
	::run_word_idx(word_idx, state);
}
void Runner::run_primitive_idx(primitive_idx_t primitive_idx) {
	assert(primitive_idx < state.primitives_len);

	const auto &primitive = state.primitives[primitive_idx];
	if (primitive.complex) {
		primitive.run(*this);
	} else {
		primitive.fun(state);
	}
}
void Runner::run_number(number_t number) {
	::run_number(number, state);
}
void Runner::run_function_ptr(function_ptr_t function_ptr) {
	::run_function_ptr(function_ptr, state);
}

bool Runner::run_next() {
	const auto value = read_value();
	if (value.has_value()) {
		run_value(value.value());
		return true;
	} else return false;
}

void Runner::ignore_word_idx(word_idx_t) { }
void Runner::ignore_primitive_idx(primitive_idx_t) { }
void Runner::ignore_number(number_t) { }
void Runner::ignore_function_ptr(function_ptr_t function_ptr) {
	if (function_ptr->ignore != nullptr) {
		function_ptr->ignore(*this);
	}
}

bool Runner::ignore_next() {
	const auto value = read_value();
	if (value.has_value()) {
		ignore_value(value.value());
		return true;
	} else return false;
}

Primitive primitives[] = {
	{ "+", "some description", { .fun = [](ProgramState &state) {
		const number_t a = *state.stack.rbegin();
		state.stack.pop_back();
		const number_t b = *state.stack.rbegin();
		state.stack.pop_back();
		const number_t res = {
			.pos = a.pos + b.pos
		};
		state.stack.push_back(res);
	} } },
	{ ".", "some description", { .fun = [](ProgramState &state) {
		if (state.stack.size() == 0) {
			std::cout << "empty." << std::endl;
		} else {
			for (const auto number : state.stack) {
				std::cout << number.sign << ' ';
			}
			std::cout << std::endl;
		}
	} } },
};

int main() {
	ProgramState state {
		.stack = {},
		.code = {},
		.error = nullptr,

		.words = {},
		.primitives = primitives,
		.primitives_len = sizeof(primitives)/sizeof(*primitives),
	};

	Interpreter interpreter {
		.line = "1 2 + .",
		.len = 7,
		.action = Interpreter::Run,
		.curr_word = {},
		.state = state,
	};

	while (!state.error && interpreter.len > 0) {
		interpreter.advance();
	}
}
