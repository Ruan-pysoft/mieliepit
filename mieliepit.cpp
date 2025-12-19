#ifdef KERNEL
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "vga.hpp"
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

/*** SECTION: Basic runner functions ***/

void run_word_idx(idx_t word_idx, ProgramState &state);
void run_primitive_idx(idx_t primitive_idx, ProgramState &state);
void run_number(number_t number, ProgramState &state);
void run_function_ptr(function_ptr_t function_ptr, Runner &runner);

void run_word_idx(idx_t word_idx, ProgramState &state) {
	assert(word_idx < length(state.words));

	const auto &word = state.words[word_idx];
	assert(word.code_pos <= length(state.code));
	assert(word.code_pos + word.code_len <= length(state.code));

	Runner runner = { {
		.code = &state.code[word.code_pos],
		.len = word.code_len,
	}, state, Runner::Run };

	while (!state.error && runner.curr.len > 0) {
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
void run_function_ptr(function_ptr_t function_ptr, Runner &runner) {
	function_ptr->run(runner);
}

}

/*** SECTION: Interpreter implementation ***/

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

	push(state.code, { .type = Value::Primitive, .primitive_idx = primitive_idx });
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

/*** SECTION: Runner implementation ***/

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
	mieliepit::run_function_ptr(function_ptr, *this);
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

namespace {

/*** SECTION: Syntax implementation ***/

void ignore_comment(Interpreter &interpreter) {
	while (true) {
		interpreter.get_word();

		if (interpreter.curr_word.len == 1 && interpreter.curr_word.text[0] == ')') {
			interpreter.curr_word.handled = true;
			break;
		} else if (interpreter.curr_word.len == 0) {
			interpreter.state.error = "Error: unclosed comment";
			interpreter.state.error_handled = false;
			break;
		}

		interpreter.ignore_next();
	}
}

struct string_value_t {
	const char *start;
	size_t len;
	size_t words;
};

string_value_t parse_string(Interpreter &interpreter) {
	interpreter.get_word();
	interpreter.curr_word.handled = true;

	const char *start = interpreter.curr_word.text;
	const char *end = start + interpreter.curr_word.len;

	while (true) {
		interpreter.get_word();
		interpreter.curr_word.handled = true;

		if (interpreter.curr_word.len == 1 && interpreter.curr_word.text[0] == '"') {
			break;
		} else if (interpreter.curr_word.len == 0) {
			interpreter.state.error = "Error: unclosed string";
			interpreter.state.error_handled = false;
			break;
		}

		end = interpreter.curr_word.text + interpreter.curr_word.len;
	}

	const size_t len = end - start;
	const size_t words = len/4 + !!(len&3);

	return {
		.start = start,
		.len = len,
		.words = words,
	};
}

void interpret_string(Interpreter &interpreter) {
	const string_value_t str = parse_string(interpreter);
	if (interpreter.state.error != nullptr) return;

	// TODO:
	// check_stack_cap("\"", words+1);

	for (size_t i = 0; i < str.words; ++i) {
		uint32_t num = 0;
		size_t j = i*4 + 4;
		if (j > str.len) j = str.len;
		while (j --> i*4) {
			const char ch = str.start[j];
			num <<= 8;
			num |= *(const uint8_t*)&ch;
		}
		push(interpreter.state.stack, { .pos = num });
	}
	push(interpreter.state.stack, { .pos = str.words });
}

void ignore_string(Interpreter &interpreter) {
	parse_string(interpreter);
}

maybe_t<size_t> compile_string(Interpreter &interpreter) {
	const string_value_t str = parse_string(interpreter);
	if (interpreter.state.error != nullptr) return {};

	const size_t start_len = length(interpreter.state.code);

	// TODO:
	// check_code_len("\"", (words+1)*2);

	for (size_t i = 0; i < str.words; ++i) {
		uint32_t num = 0;
		size_t j = i*4 + 4;
		if (j > str.len) j = str.len;
		while (j --> i*4) {
			const char ch = str.start[j];
			num <<= 8;
			num |= *(const uint8_t*)&ch;
		}
		push(interpreter.state.code, Value::new_number({ .pos = num }));
	}
	push(interpreter.state.code, Value::new_number({ .pos = str.words }));

	return length(interpreter.state.code) - start_len;
}

number_t parse_hex(Interpreter &interpreter) {
	interpreter.get_word();

	if (interpreter.curr_word.len == 0) {
		// TODO:
		// error_fun("hex", "expected a hexadecimal number, didn't get anything");
		interpreter.state.error = "Error: expected hex number after `hex`";
		interpreter.state.error_handled = false;
		return { .pos = 0 };
	}
	if (interpreter.curr_word.len > 8) {
		// TODO:
		// error_fun("hex", "largest supported number is FFFFFFFF");
		interpreter.state.error = "Error: hex number can't be larger than FFFFFFFF";
		interpreter.state.error_handled = false;
		return { .pos = 0 };
	}
	uint32_t num = 0;
	for (size_t i = 0; i < interpreter.curr_word.len; ++i) {
		const char ch = interpreter.curr_word.text[i];
		if ((ch < '0' || '9' < ch) && (ch < 'a' || 'z' < ch) && (ch < 'A' || 'Z' < ch)) {
			// TODO:
			// error_fun("hex", "expected hex number to consist only of hex digits (0-9, a-f, A-F)");
			interpreter.state.error = "Error: expected hex number to exist of only hex digits";
			interpreter.state.error_handled = false;
			return { .pos = 0 };
		}
		num <<= 4;
		if ('0' <= ch && ch <= '9') {
			num |= ch-'0';
		} else if ('a' <= ch && ch <= 'z') {
			num |= ch-'a' + 10;
		} else {
			num |= ch-'A' + 10;
		}
	}

	interpreter.curr_word.handled = true;
	return { .pos = num };
}

void interpret_hex(Interpreter &interpreter) {
	const number_t num = parse_hex(interpreter);
	if (interpreter.state.error != nullptr) return;

	// TODO:
	// check_stack_cap("\"", 1);
	push(interpreter.state.stack, num);
}

void ignore_hex(Interpreter &interpreter) {
	parse_hex(interpreter);
}

maybe_t<size_t> compile_hex(Interpreter &interpreter) {
	const number_t num = parse_hex(interpreter);
	if (interpreter.state.error != nullptr) return {};

	// TODO:
	// check_code_len("\"", 1);

	push(interpreter.state.code, Value::new_number(num));

	return 1;
}

number_t parse_short_str(Interpreter &interpreter) {
	interpreter.get_word();

	if (interpreter.curr_word.len == 0) {
		// TODO:
		// error_fun("'", "expected a short string, didn't get anything");
		interpreter.state.error = "Error: expected word after `'`";
		interpreter.state.error_handled = false;
		return { .pos = 0 };
	}
	if (interpreter.curr_word.len > 8) {
		// TODO:
		// error_fun("'", "short strings may be no longer than four characters");
		interpreter.state.error = "Error: short strings may be no longer than four characters";
		interpreter.state.error_handled = false;
		return { .pos = 0 };
	}
	uint32_t num = 0;
	size_t i = interpreter.curr_word.len;
	while (i --> 0) {
		const char ch = interpreter.curr_word.text[i];
		num <<= 8;
		num |= *(const uint8_t*)&ch;
	}

	interpreter.curr_word.handled = true;
	return { .pos = num };
}

void interpret_short_str(Interpreter &interpreter) {
	const number_t str = parse_short_str(interpreter);
	if (interpreter.state.error != nullptr) return;

	// TODO:
	// check_stack_cap("\"", 1);
	push(interpreter.state.stack, str);
}

void ignore_short_str(Interpreter &interpreter) {
	parse_short_str(interpreter);
}

maybe_t<size_t> compile_short_str(Interpreter &interpreter) {
	const number_t str = parse_short_str(interpreter);
	if (interpreter.state.error != nullptr) return {};

	// TODO:
	// check_code_len("\"", 1);

	push(interpreter.state.code, Value::new_number(str));

	return 1;
}

void interpret_help(Interpreter &interpreter) {
	interpreter.get_word();

	if (interpreter.curr_word.len == 0) {
		// TODO:
		// error_fun("help", "expected following word");
		interpreter.state.error = "Error: expected following word";
		interpreter.state.error_handled = false;
		return;
	}

	const maybe_t<Value> val = interpreter.read_value();
	if (!has(val)) {
		// TODO:
		// error_fun("help", "Couldn't find specified word");
		interpreter.state.error = "Error: couldn't find the specified word";
		interpreter.state.error_handled = false;
		return;
	}

	switch (get(val).type) {
		case Value::Word: {
			const auto &word = interpreter.state.words[get(val).word_idx];
			printf("`%s`: %s", word.name, word.desc);
		} break;
		case Value::Primitive: {
			const auto &primitive = interpreter.state.primitives[get(val).primitive_idx];
			printf("`%s`: %s", primitive.name, primitive.desc);
		} break;
		case Value::Syntax: {
			const auto &syntax = interpreter.state.syntax[get(val).syntax_idx];
			printf("`%s`: %s", syntax.name, syntax.desc);
		} break;
		case Value::Number: {
			printf("Pushes the number %u to the stack", get(val).number.pos);
		} break;
		case Value::RawFunction: {
			// TODO: some sort of error (maybe?)
			assert(false && "raw function shouldn't be possible here");
		} break;
	}
}

void ignore_help(Interpreter &interpreter) {
	interpreter.get_word();

	if (interpreter.curr_word.len == 0) {
		// TODO:
		// error_fun("help", "expected following word");
		interpreter.state.error = "Error: expected following word";
		interpreter.state.error_handled = false;
		return;
	}
}

extern RawFunction print_raw;
maybe_t<size_t> compile_help(Interpreter &interpreter) {
	interpreter.get_word();

	const size_t start_len = length(interpreter.state.code);

	if (interpreter.curr_word.len == 0) {
		// TODO:
		// error_fun("help", "expected following word");
		interpreter.state.error = "Error: expected following word";
		interpreter.state.error_handled = false;
		return {};
	}

	const maybe_t<Value> val = interpreter.read_value();
	if (!has(val)) {
		// TODO:
		// error_fun("help", "Couldn't find specified word");
		interpreter.state.error = "Error: couldn't find the specified word";
		interpreter.state.error_handled = false;
		return {};
	}

	idx_t pstr_idx = 0;
	idx_t print_idx = 0;

	{
		for (idx_t i = 0; i < interpreter.state.primitives_len; ++i) {
			if (strcmp(interpreter.state.primitives[i].name, "pstr") == 0) {
				pstr_idx = i;
			}
			if (strcmp(interpreter.state.primitives[i].name, "print") == 0) {
				print_idx = i;
			}
		}
	}

	const char *s1 = "`: ";
	const char *s2 = "Pushes the number ";
	const char *s3 = " to the stack";

	switch (get(val).type) {
		case Value::Word: {
			// TODO:
			// check_code_len("help", 15);
			const auto &word = interpreter.state.words[get(val).word_idx];

			push(interpreter.state.code, Value::new_number({
				.pos = '`'
			}));
			push(interpreter.state.code, Value::new_primitive(pstr_idx));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(word.name)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(s1)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(word.desc)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));
		} break;
		case Value::Primitive: {
			// TODO:
			// check_code_len("help", 15);
			const auto &primitive = interpreter.state.primitives[get(val).primitive_idx];

			push(interpreter.state.code, Value::new_number({
				.pos = '`'
			}));
			push(interpreter.state.code, Value::new_primitive(pstr_idx));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(primitive.name)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(s1)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(primitive.desc)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));
		} break;
		case Value::Syntax: {
			const auto &syntax = interpreter.state.syntax[get(val).syntax_idx];

			push(interpreter.state.code, Value::new_number({
				.pos = '`'
			}));
			push(interpreter.state.code, Value::new_primitive(pstr_idx));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(syntax.name)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(s1)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(syntax.desc)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));
		} break;
		case Value::Number: {
			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(s2)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number(get(val).number));
			push(interpreter.state.code, Value::new_primitive(print_idx));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(s3)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));
		} break;
		case Value::RawFunction: {
			// TODO: some sort of error (maybe?)
			assert(false && "raw function shouldn't be possible here");
		} break;
	}

	return length(interpreter.state.code) - start_len;
}

void print_definition(ProgramState &state, idx_t word_idx) {
	assert(word_idx < length(state.words));
	const Word &word = state.words[word_idx];

	printf(": %s ( %s )", word.name, word.desc);

	assert(word.code_pos < length(state.code));
	assert(word.code_pos + word.code_len < length(state.code));
	for (idx_t i = word.code_pos; i < word.code_pos + word.code_len; ++i) {
		const auto value = state.code[i];
		switch (value.type) {
			case Value::Word: {
				assert(value.word_idx < length(state.words));
				printf(" %s", state.words[value.word_idx].name);
			} break;
			case Value::Primitive: {
				assert(value.primitive_idx < state.primitives_len);
				printf(" %s", state.primitives[value.primitive_idx]);
			} break;
			case Value::Syntax: {
				state.error = "Error: syntax expression shouldn't be present in compiled word";
				state.error_handled = false;
			} break;
			case Value::Number: {
				printf(" %u", value.number.pos);
			} break;
			case Value::RawFunction: {
				printf(" %s", value.function_ptr->name);
			} break;
		}
	}
	printf(" ;");
}
void interpret_def(Interpreter &interpreter) {
	interpreter.get_word();

	if (interpreter.curr_word.len == 0) {
		// TODO:
		// error_fun("def", "expected following word");
		interpreter.state.error = "Error: expected following word";
		interpreter.state.error_handled = false;
		return;
	}

	const maybe_t<Value> val = interpreter.read_value();
	if (!has(val)) {
		// TODO:
		// error_fun("def", "Couldn't find specified word");
		interpreter.state.error = "Error: couldn't find the specified word";
		interpreter.state.error_handled = false;
		return;
	}

	switch (get(val).type) {
		case Value::Word: {
			print_definition(interpreter.state, get(val).word_idx);
		} break;
		case Value::Primitive: {
			const auto &primitive = interpreter.state.primitives[get(val).primitive_idx];
			printf("<built-in primitive `%s`>", primitive.name);
		} break;
		case Value::Syntax: {
			const auto &syntax = interpreter.state.syntax[get(val).syntax_idx];
			printf("<built-in syntax expression `%s`>", syntax.name);
		} break;
		case Value::Number: {
			printf("<literal %u>", get(val).number.pos);
		} break;
		case Value::RawFunction: {
			// TODO: some sort of error (maybe?)
			assert(false && "raw function shouldn't be possible here");
		} break;
	}
}

void ignore_def(Interpreter &interpreter) {
	interpreter.get_word();

	if (interpreter.curr_word.len == 0) {
		// TODO:
		// error_fun("def", "expected following word");
		interpreter.state.error = "Error: expected following word";
		interpreter.state.error_handled = false;
		return;
	}
}

extern RawFunction print_definition_rf;
maybe_t<size_t> compile_def(Interpreter &interpreter) {
	interpreter.get_word();

	const size_t start_len = length(interpreter.state.code);

	if (interpreter.curr_word.len == 0) {
		// TODO:
		// error_fun("def", "expected following word");
		interpreter.state.error = "Error: expected following word";
		interpreter.state.error_handled = false;
		return {};
	}

	const maybe_t<Value> val = interpreter.read_value();
	if (!has(val)) {
		// TODO:
		// error_fun("def", "Couldn't find specified word");
		interpreter.state.error = "Error: couldn't find the specified word";
		interpreter.state.error_handled = false;
		return {};
	}

	idx_t print_idx = 0;

	{
		for (idx_t i = 0; i < interpreter.state.primitives_len; ++i) {
			if (strcmp(interpreter.state.primitives[i].name, "print") == 0) {
				print_idx = i;
			}
		}
	}

	const char *s_end = "`>";
	const char *s_pri = "<built-in primitive `";
	const char *s_syn = "<built-in syntax expression `";
	const char *s_lit = "<literal ";

	switch (get(val).type) {
		case Value::Word: {
			// TODO:
			// check_code_len("def", ???);

			push(interpreter.state.code, Value::new_number({
				.pos = get(val).word_idx
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_definition_rf));
		} break;
		case Value::Primitive: {
			// TODO:
			// check_code_len("def", ???);
			const auto &primitive = interpreter.state.primitives[get(val).primitive_idx];

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(s_pri)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(primitive.name)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(s_end)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));
		} break;
		case Value::Syntax: {
			// TODO:
			// check_code_len("def", ???);

			const auto &syntax = interpreter.state.syntax[get(val).syntax_idx];

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(s_syn)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(syntax.name)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(s_end)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));
		} break;
		case Value::Number: {
			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(s_lit)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number(get(val).number));
			push(interpreter.state.code, Value::new_primitive(print_idx));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<uint32_t>(s_end)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));
		} break;
		case Value::RawFunction: {
			// TODO: some sort of error (maybe?)
			assert(false && "raw function shouldn't be possible here");
		} break;
	}

	return length(interpreter.state.code) - start_len;
}

extern RawFunction recurse;
maybe_t<size_t> compile_rec(Interpreter &interpreter) {
	// TODO:
	// check_code_len("rec", 1);

	push(interpreter.state.code, Value::new_function_ptr(&recurse));

	return 1;
}

extern RawFunction return_rf;
maybe_t<size_t> compile_ret(Interpreter &interpreter) {
	// TODO:
	// check_code_len("rec", 1);

	push(interpreter.state.code, Value::new_function_ptr(&return_rf));

	return 1;
}

void interpret_skip(Interpreter &interpreter) {
	// TODO:
	// check_stack_len_ge("?", 1);

	if (pop(interpreter.state.stack).pos == 0) {
		assert(interpreter.ignore_next()); // TODO: some sort of error or something?
	} else {
		assert(interpreter.run_next()); // TODO: some sort of error or something?
	}
}

void ignore_skip(Interpreter &interpreter) {
	assert(interpreter.ignore_next()); // TODO: some sort of error or something?
}

extern RawFunction skip;
maybe_t<size_t> compile_skip(Interpreter &interpreter) {
	// TODO:
	// check_code_len ...

	push(interpreter.state.code, Value::new_number({ .pos = 0 }));
	Value &skip_len = interpreter.state.code[length(interpreter.state.code)-1];

	push(interpreter.state.code, Value::new_function_ptr(&skip));

	const auto next_len = interpreter.compile_next();
	if (has(next_len)) {
		skip_len.number.pos = get(next_len);

		return get(next_len)+2;
	} else {
		assert(false); // TODO: some sort of error or something?

		return 2;
	}
}

/*** SECTION: Raw function values ***/

RawFunction print_raw = { "<internal:print_raw>", [](Runner &runner) {
	// TODO:
	// check_stack_len_ge("<internal:print_raw>", 1);
	const char *str = reinterpret_cast<const char*>(pop(runner.state.stack).pos);
	#ifdef KERNEL
	term::writestring(str);
	#else
	fputs(stdout, str);
	#endif
} };

RawFunction print_definition_rf = { "<internal:print_definition>", [](Runner &runner) {
	// TODO:
	// check_stack_len_ge("<internal:print_definition>", 1);
	const idx_t word_idx = pop(runner.state.stack).pos;
	print_definition(runner.state, word_idx);
} };

RawFunction recurse = { "rec", [](Runner &runner) {
	runner.curr = runner.initial;
} };

RawFunction return_rf = { "ret", [](Runner &runner) {
	runner.curr.code += runner.curr.len;
	runner.curr.len = 0;
} };

RawFunction skip = { "?", [](Runner &runner) {
	// TODO:
	// check_stack_len_ge("?", 2);

	const size_t skip_len = pop(runner.state.stack).pos;

	if (pop(runner.state.stack).pos == 0) {
		assert(skip_len >= runner.curr.len);
		runner.curr.code += skip_len;
		runner.curr.len -= skip_len;
	}
} };

}

/*** SECTION: Syntax Array ***/

const Syntax syntax[] = {
	/* VALUES */
	{ "\"", "-- ... n ; pushes a string to the top of the stack (data then length)",
		interpret_string, ignore_string, compile_string
	},
	{ "hex", "-- a ; interprets next word as hex number and pushes it",
		interpret_hex, ignore_hex, compile_hex,
	},
	{ "'", "-- a ; interprets next word as short (<= 4 long) string and pushes it",
		interpret_short_str, ignore_short_str, compile_short_str,
	},

	/* DOCUMENTATION / HELP / INSPECTION */
	{ "help", "-- ; prints help text for the next word",
		interpret_help, ignore_help, compile_help,
	},
	{ "def", "-- ; prints the definition of a given word",
		interpret_def, ignore_def, compile_def,
	},

	/* INTERNALS / SYNTAX */
	{ "(", "-- ; begins a comment", ignore_comment, ignore_comment,
		[](Interpreter &interpreter) -> maybe_t<size_t> {
			ignore_comment(interpreter);
			return 0;
		}
	},
	{ "rec", "-- ; recurses (runs the current word from the start)",
		[](Interpreter &interpreter) {
			// TODO:
			// error_fun("rec", "rec is only valid when defining a word (inside : ; )");
			interpreter.state.error = "rec is only valid when defining a word";
			interpreter.state.error_handled = false;
		}, [](Interpreter&) {}, compile_rec,
	},
	{ "ret", "-- ; returns (exits the current word early)",
		[](Interpreter &interpreter) {
			// TODO:
			// error_fun("ret", "ret is only valid when defining a word (inside : ; )");
			interpreter.state.error = "ret is only valid when defining a word";
			interpreter.state.error_handled = false;
		}, [](Interpreter&) {}, compile_ret,
	},
	{ "?", "a -- ; only executes the next word if the stack top is nonzero",
		interpret_skip, ignore_skip, compile_skip,
	},
	/*{ "rep_and", "n -- ??? n ; repeat the next word n times, and push n to the stack",
		interpret_rep_and, ignore_rep_and, compile_rep_and,
	},
	{ "rep", "n -- ??? ; repeat the next word n times",
		interpret_rep, ignore_rep, compile_rep,
	},*/
};
const size_t syntax_len = sizeof(syntax)/sizeof(*syntax);

}
