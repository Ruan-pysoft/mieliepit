#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

namespace mieliepit {

struct ProgramState;
struct Interpreter;
struct Runner;
struct RawFunction;

using idx_t = uint64_t;
static_assert(
	sizeof(idx_t) == sizeof(std::size_t),
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

struct Word {
	const char *name;
	const char *desc;
	idx_t code_pos;
	size_t code_len;
};

struct Primitive {
	const char *name;
	const char *desc;
	void (*fun)(ProgramState&);
};

struct Syntax {
	const char *name;
	const char *desc;
	void (*run)(Interpreter&);
	void (*ignore)(Interpreter&);
	std::optional<std::size_t> (*compile)(Interpreter&);
};

struct RawFunction {
	const char *name;
	void (*run)(ProgramState&);
	void (*ignore)(Runner&) = nullptr;
};

struct Value {
	enum Type {
		Word,
		Primative,
		Syntax,
		Number,
		RawFunction,
	} type;
	union {
		uint64_t raw_value;
		idx_t word_idx;
		idx_t primitive_idx;
		idx_t syntax_idx;
		number_t number;
		function_ptr_t function_ptr;
	};
};

using Stack = std::vector<number_t>;

struct ProgramState {
	Stack stack;
	std::vector<Value> code;
	const char *error;

	std::vector<Word> words;
	const Primitive *primitives;
	const std::size_t primitives_len;
	const Syntax *syntax;
	const std::size_t syntax_len;
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

	std::optional<idx_t> read_word_idx() {
		get_word();

		if (curr_word.len == 0) return {};

		idx_t i = state.words.size();
		while (i --> 0) {
			if (strlen(state.words[i].name) != curr_word.len) continue;

			if (strncmp(state.words[i].name, curr_word.text, curr_word.len) == 0) {
				return i;
			}
		}

		unget_word();
		return {};
	}
	std::optional<idx_t> read_primitive_idx() {
		get_word();

		if (curr_word.len == 0) return {};

		idx_t i = state.primitives_len;
		while (i --> 0) {
			if (strlen(state.primitives[i].name) != curr_word.len) continue;

			if (strncmp(state.primitives[i].name, curr_word.text, curr_word.len) == 0) {
				return i;
			}
		}

		unget_word();
		return {};
	}
	std::optional<idx_t> read_syntax_idx() {
		get_word();

		if (curr_word.len == 0) return {};

		idx_t i = state.syntax_len;
		while (i --> 0) {
			if (strlen(state.syntax[i].name) != curr_word.len) continue;

			if (strncmp(state.syntax[i].name, curr_word.text, curr_word.len) == 0) {
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

		const auto syntax_idx = read_syntax_idx();
		if (syntax_idx.has_value()) return { {
			.type = Value::Syntax,
			.syntax_idx = syntax_idx.value(),
		} };

		const auto number = read_number();
		if (number.has_value()) return { {
			.type = Value::Number,
			.number = number.value(),
		} };

		state.error = "Error: undefined word";

		return {};
	}

	void run_word_idx(idx_t word_idx);
	void run_primitive_idx(idx_t primitive_idx);
	void run_syntax_idx(idx_t syntax_idx);
	void run_number(number_t number);
	void run_value(Value value) {
		switch (value.type) {
			case Value::Word: {
				run_word_idx(value.word_idx);
			} break;
			case Value::Primative: {
				run_primitive_idx(value.primitive_idx);
			} break;
			case Value::Syntax: {
				run_syntax_idx(value.syntax_idx);
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

	std::optional<std::size_t> compile_word_idx(idx_t word_idx);
	std::optional<std::size_t> compile_primitive_idx(idx_t primitive_idx);
	std::optional<std::size_t> compile_syntax_idx(idx_t syntax_idx);
	std::optional<std::size_t> compile_number(number_t number);
	std::optional<std::size_t> compile_value(Value value) {
		switch (value.type) {
			case Value::Word: {
				return compile_word_idx(value.word_idx);
			} break;
			case Value::Primative: {
				return compile_primitive_idx(value.primitive_idx);
			} break;
			case Value::Syntax: {
				return compile_syntax_idx(value.syntax_idx);
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

	void ignore_word_idx(idx_t word_idx);
	void ignore_primitive_idx(idx_t primitive_idx);
	void ignore_syntax_idx(idx_t syntax_idx);
	void ignore_number(number_t number);
	void ignore_value(Value value) {
		switch (value.type) {
			case Value::Word: {
				ignore_word_idx(value.word_idx);
			} break;
			case Value::Primative: {
				ignore_primitive_idx(value.primitive_idx);
			} break;
			case Value::Syntax: {
				ignore_syntax_idx(value.syntax_idx);
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

	void run_word_idx(idx_t word_idx);
	void run_primitive_idx(idx_t primitive_idx);
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
			case Value::Syntax: {
				state.error = "Error: cannot run compiled syntax expression";
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

	void ignore_word_idx(idx_t word_idx);
	void ignore_primitive_idx(idx_t primitive_idx);
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
			case Value::Syntax: {
				state.error = "Error: cannot run compiled syntax expression";
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

extern const Syntax syntax[];
extern const std::size_t syntax_len;

}
