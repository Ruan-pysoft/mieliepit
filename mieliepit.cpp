#ifdef KERNEL
#include <assert.h>
#else
#include <cassert>
#include <cstddef>
#include <cstring>
#include <optional>
#include <vector>
#endif

#include "./mieliepit.hpp"

namespace mieliepit {

namespace {

void run_word_idx(idx_t word_idx, ProgramState &state);
void run_primitive_idx(idx_t primitive_idx, ProgramState &state);
void run_number(number_t number, ProgramState &state);
void run_function_ptr(function_ptr_t function_ptr, ProgramState &state);

void run_word_idx(idx_t word_idx, ProgramState &state) {
	assert(word_idx < length(state.words));

	const auto &word = state.words[word_idx];
	assert(word.code_pos <= length(state.code));
	assert(word.code_pos + word.code_len <= length(state.code));

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
void run_primitive_idx(idx_t primitive_idx, ProgramState &state) {
	assert(primitive_idx < state.primitives_len);

	const auto &primitives = state.primitives[primitive_idx];
	primitives.fun(state);
}
void run_number(number_t number, ProgramState &state) {
	push(state.stack, number);
}
void run_function_ptr(function_ptr_t function_ptr, ProgramState &state) {
	function_ptr->run(state);
}

}

void Interpreter::run_word_idx(idx_t word_idx) {
	mieliepit::run_word_idx(word_idx, state);
}
void Interpreter::run_primitive_idx(idx_t primitive_idx) {
	mieliepit::run_primitive_idx(primitive_idx, state);
}
void Interpreter::run_syntax_idx(idx_t syntax_idx) {
	assert(syntax_idx < state.syntax_len);

	const auto &syntax = state.syntax[syntax_idx];
	syntax.run(*this);
}
void Interpreter::run_number(number_t number) {
	mieliepit::run_number(number, state);
}

bool Interpreter::run_next() {
	const auto value = read_value();
	if (has(value)) {
		run_value(get(value));
		return true;
	} else return false;
}

maybe_t<size_t> Interpreter::compile_word_idx(idx_t word_idx) {
	assert(word_idx < length(state.words));

	push(state.code, { .type = Value::Word, .word_idx = word_idx });
	return 1;
}
maybe_t<size_t> Interpreter::compile_primitive_idx(idx_t primitive_idx) {
	assert(primitive_idx < state.primitives_len);

	push(state.code, { .type = Value::Primative, .primitive_idx = primitive_idx });
	return 1;
}
maybe_t<size_t> Interpreter::compile_syntax_idx(idx_t syntax_idx) {
	assert(syntax_idx < state.syntax_len);

	const auto &syntax = state.syntax[syntax_idx];
	return syntax.compile(*this);
}
maybe_t<size_t> Interpreter::compile_number(number_t number) {
	push(state.code, { .type = Value::Number, .number = number });
	return 1;
}

maybe_t<size_t> Interpreter::compile_next() {
	const auto value = read_value();
	if (has(value)) {
		return compile_value(get(value));
	}
	return {};
}

void Interpreter::ignore_word_idx(idx_t) { }
void Interpreter::ignore_primitive_idx(idx_t) { }
void Interpreter::ignore_syntax_idx(idx_t syntax_idx) {
	assert(syntax_idx < state.syntax_len);

	const auto &syntax = state.syntax[syntax_idx];
	syntax.ignore(*this);
}
void Interpreter::ignore_number(number_t) { }

bool Interpreter::ignore_next() {
	const auto value = read_value();
	if (has(value)) {
		ignore_value(get(value));
		return true;
	} return false;
}

void Runner::run_word_idx(idx_t word_idx) {
	mieliepit::run_word_idx(word_idx, state);
}
void Runner::run_primitive_idx(idx_t primitive_idx) {
	mieliepit::run_primitive_idx(primitive_idx, state);
}
void Runner::run_number(number_t number) {
	mieliepit::run_number(number, state);
}
void Runner::run_function_ptr(function_ptr_t function_ptr) {
	mieliepit::run_function_ptr(function_ptr, state);
}

bool Runner::run_next() {
	const auto value = read_value();
	if (has(value)) {
		run_value(get(value));
		return true;
	} else return false;
}

void Runner::ignore_word_idx(idx_t) { }
void Runner::ignore_primitive_idx(idx_t) { }
void Runner::ignore_number(number_t) { }
void Runner::ignore_function_ptr(function_ptr_t function_ptr) {
	if (function_ptr->ignore != nullptr) {
		function_ptr->ignore(*this);
	}
}

bool Runner::ignore_next() {
	const auto value = read_value();
	if (has(value)) {
		ignore_value(get(value));
		return true;
	} else return false;
}

void ignore_comment(Interpreter &interpreter) {
	while (true) {
		interpreter.get_word();

		if (interpreter.curr_word.len == 1 && interpreter.curr_word.text[0] == ')') {
			interpreter.curr_word.handled = true;
			break;
		}

		interpreter.ignore_next();
	}
}

const Syntax syntax[] = {
	{ "(", "", ignore_comment, ignore_comment,
		[](Interpreter &interpreter) -> maybe_t<size_t> {
			ignore_comment(interpreter);
			return 0;
		}
	},
};
const size_t syntax_len = sizeof(syntax)/sizeof(*syntax);

}
