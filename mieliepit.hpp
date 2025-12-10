#pragma once

#ifdef KERNEL
#else
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>
#endif

namespace mieliepit {

struct ProgramState;
struct Interpreter;
struct Runner;
struct RawFunction;

#ifdef KERNEL
using idx_t = uint32_t;
static_assert(
	sizeof(idx_t) == sizeof(size_t),
	"Expected word size to be 32 bits"
);
union number_t {
	uint32_t pos;
	int32_t sign;
};
static_assert(
	sizeof(number_t) == sizeof(size_t),
	"Expected word size to be 32 bits"
);
using function_ptr_t = RawFunction *;
static_assert(
	sizeof(function_ptr_t) == sizeof(size_t),
	"Expected word size to be 32 bits"
);
#else
using idx_t = uint64_t;
static_assert(
	sizeof(idx_t) == sizeof(size_t),
	"Expected word size to be 64 bits"
);
union number_t {
	uint64_t pos;
	int64_t sign;
};
static_assert(
	sizeof(number_t) == sizeof(size_t),
	"Expected word size to be 64 bits"
);
using function_ptr_t = RawFunction *;
static_assert(
	sizeof(function_ptr_t) == sizeof(size_t),
	"Expected word size to be 64 bits"
);
#endif

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

#ifdef KERNEL
template<typename T>
using maybe_t = Maybe<T>;

template<typename T>
bool has(const maybe_t<T> &maybe) { return maybe.has; }
template<typename T>
bool get(const maybe_t<T> &maybe) { return maybe.value; }
template<typename T>
bool get_or(const maybe_t<T> &maybe, T alternative) {
	if (maybe.has) return maybe.value;
	else return alternative;
}
#else
template<typename T>
using maybe_t = std::optional<T>;

template<typename T>
bool has(const maybe_t<T> &maybe) { return maybe.has_value(); }
template<typename T>
bool get(const maybe_t<T> &maybe) { return maybe.value(); }
template<typename T>
bool get_or(const maybe_t<T> &maybe, T alternative) {
	return maybe.value_or(alternative);
}
#endif

struct Syntax {
	const char *name;
	const char *desc;
	void (*run)(Interpreter&);
	void (*ignore)(Interpreter&);
	maybe_t<size_t> (*compile)(Interpreter&);
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

#ifdef KERNEL
template<typename T, size_t CAPACITY>
struct FixedBuffer {
	size_t len = 0;
	T buffer[CAPACITY];

	void push(T value) {
		assert(len < CAPACITY);

		buffer[len++] = value;
	}

	T pop() {
		assert(len > 0);

		return buffer[--len];
	}

	const T &operator[](idx_t idx) const {
		assert(idx < len);

		return buffer[idx];
	}
	T &operator[](idx_t idx) {
		assert(idx < len);

		return buffer[idx];
	}
};

template<typename T, size_t CAPACITY>
void push(FixedBuffer<T, CAPACITY> &buf, T value) {
	buf.push(value);
}
template<typename T, size_t CAPACITY>
T pop(FixedBuffer<T, CAPACITY> &buf) {
	return buf.pop();
}
template<typename T, size_t CAPACITY>
size_t lenght(const FixedBuffer<T, CAPACITY> &buf) {
	return buf.len;
}
#else
template<typename T>
void push(std::vector<T> &vec, T value) {
	vec.push(value);
}
template<typename T>
T pop(std::vector<T> &vec) {
	return vec.pop();
}
template<typename T>
size_t length(const std::vector<T> &vec) {
	return vec.size();
}
#endif

#ifdef KERNEL
constexpr size_t STACK_SIZE = 1024;
using Stack = FixedBuffer<number_t, STACK_SIZE>;
#else
using Stack = std::vector<number_t>;
#endif
static constexpr const number_t &stack_peek(const Stack &stack, idx_t nth = 0) {
	return stack[length(stack)-1 - nth];
}

#ifdef KERNEL
constexpr size_t CODE_BUFFER_SIZE = 1024;
using CodeCodeBuffer = FixedBuffer<Value, CODE_BUFFER_SIZE>;
#else
using CodeBuffer = std::vector<Value>;
#endif

#ifdef KERNEL
constexpr size_t WORDS_SIZE = 1024;
using Words = FixedBuffer<Word, CODE_BUFFER_SIZE>;
#else
using Words = std::vector<Word>;
#endif

struct ProgramState {
	Stack stack;
	CodeBuffer code;
	const char *error;

	Words words;
	const Primitive *primitives;
	const size_t primitives_len;
	const Syntax *syntax;
	const size_t syntax_len;
};

struct Interpreter {
	const char *line = nullptr;
	size_t len = 0;
	enum {
		Run,
		Compile,
		Ignore,
	} action;
	struct {
		const char *text;
		size_t len;
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

	maybe_t<idx_t> read_word_idx() {
		get_word();

		if (curr_word.len == 0) return {};

		idx_t i = length(state.words);
		while (i --> 0) {
			if (strlen(state.words[i].name) != curr_word.len) continue;

			if (strncmp(state.words[i].name, curr_word.text, curr_word.len) == 0) {
				return i;
			}
		}

		unget_word();
		return {};
	}
	maybe_t<idx_t> read_primitive_idx() {
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
	maybe_t<idx_t> read_syntax_idx() {
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
	maybe_t<number_t> read_number() {
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
	maybe_t<Value> read_value() {
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

	maybe_t<size_t> compile_word_idx(idx_t word_idx);
	maybe_t<size_t> compile_primitive_idx(idx_t primitive_idx);
	maybe_t<size_t> compile_syntax_idx(idx_t syntax_idx);
	maybe_t<size_t> compile_number(number_t number);
	maybe_t<size_t> compile_value(Value value) {
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

	maybe_t<size_t> compile_next();

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
	size_t len = 0;
	enum {
		Run,
		Ignore,
	} action;
	ProgramState &state;

	maybe_t<Value> read_value() {
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
extern const size_t syntax_len;

}
