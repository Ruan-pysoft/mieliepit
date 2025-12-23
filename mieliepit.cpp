#ifdef KERNEL
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vga.hpp"
#else
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <vector>
#endif

#include "./mieliepit.hpp"

namespace {

void writestring(const char *str) {
#ifdef KERNEL
	term::writestring(str);
#else
	std::cout << str;
#endif
}

void writestringl(const char *str) {
#ifdef KERNEL
	puts(str);
#else
	std::cout << str << std::endl;
#endif
}

void writechar(char ch) {
#ifdef KERNEL
	term::putchar(ch);
#else
	std::cout << ch;
#endif
}

#ifdef KERNEL
using ssize_t = int32_t;
static_assert(sizeof(ssize_t) == sizeof(size_t));
#endif

}

namespace mieliepit {

ProgramState::ProgramState(const Primitive *primitives, size_t primitives_len, const Syntax *syntax, size_t syntax_len)
: primitives(primitives), primitives_len(primitives_len), syntax(syntax), syntax_len(syntax_len)
{
	word_names_buf.first = (char(*)[WORD_NAMES_BUF_SIZE])malloc(sizeof(*word_names_buf.first));
	word_descs_buf.first = (char(*)[WORD_DESCS_BUF_SIZE])malloc(sizeof(*word_descs_buf.first));
}
ProgramState::~ProgramState() {
	free(word_names_buf.first);
	free(word_descs_buf.first);
}

void ProgramState::define_word(const char *name, size_t name_len, const char *desc, size_t desc_len, idx_t code_pos, size_t code_len) {
	// TODO: proper errors
	assert(word_names_buf.second + name_len + 1 <= WORD_NAMES_BUF_SIZE);
	assert(word_descs_buf.second + desc_len + 1 <= WORD_DESCS_BUF_SIZE);

	for (idx_t i = 0; i < name_len; ++i) {
		(*word_names_buf.first)[word_names_buf.second + i] = name[i];
	}
	(*word_names_buf.first)[word_names_buf.second + name_len] = 0;
	const char *stored_name = &(*word_names_buf.first)[word_names_buf.second];
	word_names_buf.second += name_len + 1;

	for (idx_t i = 0; i < desc_len; ++i) {
		(*word_descs_buf.first)[word_descs_buf.second + i] = desc[i];
	}
	(*word_descs_buf.first)[word_descs_buf.second + desc_len] = 0;
	const char *stored_desc = &(*word_descs_buf.first)[word_descs_buf.second];
	word_descs_buf.second += desc_len + 1;

	Word word = {
		.name = stored_name,
		.desc = stored_desc,
		.code_pos = code_pos,
		.code_len = code_len,
	};

	push(words, word);
}

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
	}, state };

	while (!state.error && runner.curr.len > 0) {
		runner.run_next();
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

/*** SECTION: Primitives Array ***/

const char *guide_text =
	"Mieliepit is a stack-based programming language.\n"
	"It is operated by entering a sequence of space-seperated words into the prompt.\n"
	"Data consists of 32-bit integers stored on a stack. You can enter a single period ( . ) into the prompt at any time to view the stack.\n"
	"Comments are formed with parentheses: ( this is a comment ) . Remember to leave spaces around each parenthesis!\n"
	"There are two kinds of words: Primitives, which perform some operation, and numbers, which pushes a number to the stack.\n"
	"To get a list of available primitives, enter `primitives` into the prompt.\n"
	"To get more information on a given primitive, enter `help` followed by its name. Try `help help` or `help guide`.\n"
	"A simple hello world program is `' hell pstr ' o pstr 32 pstr ' worl pstr ' d! pstr`. See if you can figure out how it works.\n"
	"To get a list of available words, enter `words` into the prompt.\n"
	"To see what a word was compiled into, enter `def` followed by its name. Try `def neg`.\n"
	"To define your own word, start with `:`, followed by its name, then some documentation in a comment, then its code, ending off with `;`.\n"
	"An example word definition would be : test ( this is an example ) ' test pstr ; . See if you can define your own plus function using `-` and `neg`.\n"
;

#define error(msg) do { \
		state.error = msg; \
		state.error_handled = false; \
		return; \
	} while (0)
#define error_fun(fun, msg) error("Error in `" fun "`: " msg)
#define check_stack_len_lt(fun, expr) if (length(state.stack) >= (expr)) error_fun(fun, "stack length should be < " #expr)
#define check_stack_len_ge(fun, expr) if (length(state.stack) < (expr)) error_fun(fun, "stack length should be >= " #expr)
#ifdef KERNEL
#define check_stack_cap(fun, expr) if (length(state.stack) + (expr) >= STACK_SIZE) error_fun(fun, "stack capacity should be at least " #expr)
#else
#define check_stack_cap(fun, expr) do {} while (0)
#endif
#define check_code_len(fun, len) if (length(state.code) + (len) > CODE_BUFFER_SIZE) error_fun(fun, "not enough space to generate code for user word")
using pstate_t = ProgramState;
const Primitive primitives[PW_COUNT] = {
	/* STACK OPERATIONS */
	[PW_ShowStack] = { ".", "-- ; shows the top 16 elements of the stack", [](pstate_t &state) {
		if (length(state.stack) == 0) { writestringl("empty."); return; }

		const size_t amt = length(state.stack) < 16
			? length(state.stack)
			: 16;
		if (length(state.stack) > 16) {
			writestring("... ");
		}
		size_t i = amt;
		while (i --> 0) {
		#ifdef KERNEL
			printf("%d ", stack_peek(state.stack, i).sign);
		#else
			std::cout << stack_peek(state.stack, i).sign << " ";
		#endif
		}
		writechar('\n');
	} },
	[PW_StackLen] = { "stack_len", "-- a ; pushes length of stack", [](pstate_t &state) {
		check_stack_cap("stack_len", 1);
		push(state.stack, {
			.pos = length(state.stack)
		});
	} },
	[PW_Dup] = { "dup", "a -- a a", [](pstate_t &state) {
		check_stack_len_ge("dup", 1);
		check_stack_cap("dup", 1);
		push(state.stack, stack_peek(state.stack));
	} },
	[PW_Swap] = { "swap", "a b -- b a", [](pstate_t &state) {
		check_stack_len_ge("swap", 2);
		const number_t top = pop(state.stack);
		const number_t under_top = pop(state.stack);
		push(state.stack, top);
		push(state.stack, under_top);
	} },
	[PW_Rot] = { "rot", "a b c -- b c a", [](pstate_t &state) {
		check_stack_len_ge("rot", 3);
		const number_t c = pop(state.stack);
		const number_t b = pop(state.stack);
		const number_t a = pop(state.stack);
		push(state.stack, b);
		push(state.stack, c);
		push(state.stack, a);
	} },
	[PW_Unrot] = { "unrot", "a b c -- c a b", [](pstate_t &state) {
		check_stack_len_ge("rot", 3);
		const number_t c = pop(state.stack);
		const number_t b = pop(state.stack);
		const number_t a = pop(state.stack);
		push(state.stack, c);
		push(state.stack, a);
		push(state.stack, b);
	} },
	[PW_Rev] = { "rev", "a b c -- c b a", [](pstate_t &state) {
		check_stack_len_ge("rev", 3);
		const number_t c = pop(state.stack);
		const number_t b = pop(state.stack);
		const number_t a = pop(state.stack);
		push(state.stack, c);
		push(state.stack, b);
		push(state.stack, a);
	} },
	[PW_Drop] = { "drop", "a --", [](pstate_t &state) {
		check_stack_len_ge("drop", 1);
		pop(state.stack);
	} },
	[PW_RevN] = { "rev_n", "... n -- ... ; reverse the top n elements", [](pstate_t &state) {
		check_stack_len_ge("rev_n", 1);
		const size_t n = pop(state.stack).pos;
		check_stack_len_ge("rot_n", n);
		for (size_t i = 0; i < n/2; ++i) {
			const size_t fst_ix = i;
			const size_t scd_ix = n-i-1;
			const number_t tmp = stack_peek(state.stack, fst_ix);
			stack_peek(state.stack, fst_ix) = stack_peek(state.stack, scd_ix);
			stack_peek(state.stack, scd_ix) = tmp;
		}
	} },
	[PW_Nth] = { "nth", "... n -- ... x ; dup the nth element down to the top", [](pstate_t &state) {
		check_stack_len_ge("nth", 1);
		const size_t n = pop(state.stack).pos;
		check_stack_len_ge("nth", n);
		if (n == 0) {
			error_fun("nth", "n must be nonzero");
		}
		push(state.stack, stack_peek(state.stack, n-1));
	} },

	/* ARYTHMETIC OPERATIONS */
	[PW_Inc] = { "inc", "a -- a+1", [](pstate_t &state) {
		check_stack_len_ge("inc", 1);
		++stack_peek(state.stack).pos;
	} },
	[PW_Dec] = { "dec", "a -- a-1", [](pstate_t &state) {
		check_stack_len_ge("dec", 1);
		--stack_peek(state.stack).pos;
	} },
	[PW_Add] = { "+", "a b -- a+b", [](pstate_t &state) {
		check_stack_len_ge("+", 2);
		push(state.stack, {
			.pos = pop(state.stack).pos + pop(state.stack).pos,
		});
	} },
	[PW_Mul] = { "*", "a b -- a*b", [](pstate_t &state) {
		check_stack_len_ge("*", 2);
		push(state.stack, {
			.sign = pop(state.stack).sign * pop(state.stack).sign,
		});
	} },
	[PW_Div] = { "/", "a b -- a/b", [](pstate_t &state) {
		check_stack_len_ge("/", 2);
		const number_t b = pop(state.stack);
		const number_t a = pop(state.stack);
		push(state.stack, {
			.sign = a.sign / b.sign,
		});
	} },

	/* BITWISE OPERATIONS */
	[PW_Shl] = { "shl", "a b -- a<<b", [](pstate_t &state) {
		check_stack_len_ge("shl", 2);
		const size_t top = pop(state.stack).pos;
		const size_t under_top = pop(state.stack).pos;
		if (top >= 32) {
			push(state.stack, {0});
		} else {
			push(state.stack, { .pos = under_top << top });
		}
	} },
	[PW_Shr] = { "shr", "a b -- a>>b", [](pstate_t &state) {
		check_stack_len_ge("shr", 2);
		const size_t top = pop(state.stack).pos;
		const size_t under_top = pop(state.stack).pos;
		if (top >= 32) {
			push(state.stack, {0});
		} else {
			push(state.stack, { .pos = under_top >> top });
		}
	} },
	[PW_Or] = { "or", "a b -- a|b", [](pstate_t &state) {
		check_stack_len_ge("or", 2);
		push(state.stack, {
			.pos = pop(state.stack).pos | pop(state.stack).pos
		});
	} },
	[PW_And] = { "and", "a b -- a&b", [](pstate_t &state) {
		check_stack_len_ge("and", 2);
		push(state.stack, {
			.pos = pop(state.stack).pos & pop(state.stack).pos
		});
	} },
	[PW_Xor] = { "xor", "a b -- a^b", [](pstate_t &state) {
		check_stack_len_ge("xor", 2);
		push(state.stack, {
			.pos = pop(state.stack).pos ^ pop(state.stack).pos
		});
	} },
	[PW_Not] = { "not", "a -- ~a", [](pstate_t &state) {
		check_stack_len_ge("not", 1);
		push(state.stack, { .pos = ~pop(state.stack).pos });
	} },

	/* COMPARISON */
	[PW_Eq] = { "=", "a b -- a=b", [](pstate_t &state) {
		check_stack_len_ge("=?", 2);
		push(state.stack, {
			.sign = pop(state.stack).pos == pop(state.stack).pos
			? -1 : 0
		});
	} },
	[PW_Lt] = { "<", "a b -- a<b", [](pstate_t &state) {
		check_stack_len_ge("=?", 2);
		const ssize_t b = pop(state.stack).sign;
		const ssize_t a = pop(state.stack).sign;
		push(state.stack, { .sign = a < b ? -1 : 0 });
	} },

	/* LITERALS */
	[PW_True] = { "true", "-- -1", [](pstate_t &state) {
		check_stack_cap("true", 1);
		push(state.stack, { .sign = -1 });
	} },
	[PW_False] = { "false", "-- 0", [](pstate_t &state) {
		check_stack_cap("false", 1);
		push(state.stack, { .sign = 0 });
	} },

	/* OUTPUT OPERATIONS */
	[PW_Print] = { "print", "a -- ; prints top element of stack as a signed number", [](pstate_t &state) {
		check_stack_len_ge("print", 1);
		const auto top = pop(state.stack);
	#ifdef KERNEL
		printf("%d ", top.sign);
	#else
		std::cout << top.sign << " ";
	#endif
	} },
	[PW_Pstr] = { "pstr", "a -- ; prints top element as string of at most four characters", [](pstate_t &state) {
		check_stack_len_ge("pstr", 1);
		const size_t str_raw = pop(state.stack).pos;
		const char *str = (char*)&str_raw;
		constexpr size_t substr_max_width = sizeof(size_t);
		for (size_t i = 0; i < substr_max_width; ++i) {
			if (str[i] == 0) break;
			writechar(str[i]);
		}
	} },

	/* STRINGS */
	[PW_PrintString] = { "print_string", "... n -- ; prints a string of length n", [](pstate_t &state) {
		check_stack_len_ge("print_string", 1);
		const size_t n = pop(state.stack).pos;

		check_stack_len_ge("print_string", n);
		writestring((const char*)&stack_peek(state.stack, n-1));
		for (size_t i = 0; i < n; ++i) pop(state.stack);
	} },

	/* SYSTEM OPERATION */
	[PW_Exit] = { "exit", "-- ; exits the mieliepit interpreter", quit_primitive_fn },
	[PW_Quit] = { "quit", "-- ; exits the mieliepit interpreter", quit_primitive_fn },
	// TODO: sleep functions perhaps, clearing the keyboard buffer when done? Essentially ignoring all user input while sleeping

	/* DOCUMENTATION / HELP / INSPECTION */
	[PW_Syntax] = { "syntax", "-- ; prints a list of all available syntax items", [](pstate_t &state) {
		for (idx_t i = 0; i < SC_COUNT; ++i) {
			if (i) writechar(' ');
			writestring(state.syntax[i].name);
		}
		writechar('\n');
	} },
	[PW_Primitives] = { "primitives", "-- ; prints a list of all available primitive words", [](pstate_t &state) {
		for (idx_t i = 0; i < state.primitives_len; ++i) {
			if (i) writechar(' ');
			writestring(state.primitives[i].name);
		}
		writechar('\n');
	} },
	[PW_Words] = { "words", "-- ; prints a list of all user-defined words", [](pstate_t &state) {
		size_t i = length(state.words);
		while (i --> 0) {
			writestring(state.words[i].name);
			if (i) writechar(' ');
		}
		writechar('\n');
	} },
	[PW_Guide] = { "guide", "-- ; prints usage guide for the mieliepit interpreter", guide_primitive_fn },
};

#undef error_fun
#undef error

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

		interpreter.curr_word.handled = true;
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

	constexpr size_t substr_max_width = sizeof(size_t);

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
	const size_t words = len/substr_max_width + !!(len&(substr_max_width-1));

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

	constexpr size_t substr_max_width = sizeof(size_t);

	for (size_t i = 0; i < str.words; ++i) {
		size_t num = 0;
		size_t j = i*substr_max_width + substr_max_width;
		if (j > str.len) j = str.len;
		while (j --> i*substr_max_width) {
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

	constexpr size_t substr_max_width = sizeof(size_t);

	for (size_t i = 0; i < str.words; ++i) {
		size_t num = 0;
		size_t j = i*substr_max_width + substr_max_width;
		if (j > str.len) j = str.len;
		while (j --> i*substr_max_width) {
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
	size_t num = 0;
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
	constexpr size_t substr_max_width = sizeof(size_t);
	if (interpreter.curr_word.len > substr_max_width) {
		// TODO:
		// error_fun("'", "short strings may be no longer than four characters");
		#ifdef KERNEL
		interpreter.state.error = "Error: short strings may be no longer than four characters";
		#else
		interpreter.state.error = "Error: short strings may be no longer than eight characters";
		#endif
		interpreter.state.error_handled = false;
		return { .pos = 0 };
	}
	size_t num = 0;
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
		#ifdef KERNEL
			printf("`%s`: %s", word.name, word.desc);
		#else
			std::cout << '`' << word.name << "`: " << word.desc;
		#endif
		} break;
		case Value::Primitive: {
			const auto &primitive = interpreter.state.primitives[get(val).primitive_idx];
		#ifdef KERNEL
			printf("`%s`: %s", primitive.name, primitive.desc);
		#else
			std::cout << '`' << primitive.name << "`: " << primitive.desc;
		#endif
		} break;
		case Value::Syntax: {
			const auto &syntax = interpreter.state.syntax[get(val).syntax_idx];
		#ifdef KERNEL
			printf("`%s`: %s", syntax.name, syntax.desc);
		#else
			std::cout << '`' << syntax.name << "`: " << syntax.desc;
		#endif
		} break;
		case Value::Number: {
		#ifdef KERNEL
			printf("Pushes the number %u to the stack", get(val).number.pos);
		#else
			std::cout << "Pushes the number " << get(val).number.pos << " to the stack";
		#endif
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
			push(interpreter.state.code, Value::new_primitive(PW_Pstr));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(word.name)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(s1)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(word.desc)
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
			push(interpreter.state.code, Value::new_primitive(PW_Pstr));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(primitive.name)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(s1)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(primitive.desc)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));
		} break;
		case Value::Syntax: {
			const auto &syntax = interpreter.state.syntax[get(val).syntax_idx];

			push(interpreter.state.code, Value::new_number({
				.pos = '`'
			}));
			push(interpreter.state.code, Value::new_primitive(PW_Pstr));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(syntax.name)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(s1)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(syntax.desc)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));
		} break;
		case Value::Number: {
			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(s2)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number(get(val).number));
			push(interpreter.state.code, Value::new_primitive(PW_Print));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(s3)
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

#ifdef KERNEL
	printf(": %s ( %s )", word.name, word.desc);
#else
	std::cout << ": " << word.name << " ( " << word.desc << " )";
#endif

	assert(word.code_pos <= length(state.code));
	assert(word.code_pos + word.code_len <= length(state.code));
	for (idx_t i = word.code_pos; i < word.code_pos + word.code_len; ++i) {
		const auto value = state.code[i];
		switch (value.type) {
			case Value::Word: {
				assert(value.word_idx < length(state.words));
			#ifdef KERNEL
				printf(" %s", state.words[value.word_idx].name);
			#else
				std::cout << ' ' << state.words[value.word_idx].name;
			#endif
			} break;
			case Value::Primitive: {
				assert(value.primitive_idx < state.primitives_len);
			#ifdef KERNEL
				printf(" %s", state.primitives[value.primitive_idx]);
			#else
				std::cout << ' ' << state.primitives[value.primitive_idx].name;
			#endif
			} break;
			case Value::Syntax: {
				state.error = "Error: syntax expression shouldn't be present in compiled word";
				state.error_handled = false;
			} break;
			case Value::Number: {
			#ifdef KERNEL
				printf(" %u", value.number.pos);
			#else
				std::cout << ' ' << value.number.pos;
			#endif
			} break;
			case Value::RawFunction: {
			#ifdef KERNEL
				printf(" %s", value.function_ptr->name);
			#else
				std::cout << ' ' << value.function_ptr->name;
			#endif
			} break;
		}
	}
	writestring(" ;");
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
		#ifdef KERNEL
			printf("<built-in primitive `%s`>", primitive.name);
		#else
			std::cout << "<built-in primitive `" << primitive.name << "`>";
		#endif
		} break;
		case Value::Syntax: {
			const auto &syntax = interpreter.state.syntax[get(val).syntax_idx];
		#ifdef KERNEL
			printf("<built-in syntax expression `%s`>", syntax.name);
		#else
			std::cout << "<build-in syntax expression `" << syntax.name << "`>";
		#endif
		} break;
		case Value::Number: {
		#ifdef KERNEL
			printf("<literal %u>", get(val).number.pos);
		#else
			std::cout << "<literal " << get(val).number.pos << '>';
		#endif
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
				.pos = reinterpret_cast<idx_t>(s_pri)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(primitive.name)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(s_end)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));
		} break;
		case Value::Syntax: {
			// TODO:
			// check_code_len("def", ???);

			const auto &syntax = interpreter.state.syntax[get(val).syntax_idx];

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(s_syn)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(syntax.name)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(s_end)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));
		} break;
		case Value::Number: {
			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(s_lit)
			}));
			push(interpreter.state.code, Value::new_function_ptr(&print_raw));

			push(interpreter.state.code, Value::new_number(get(val).number));
			push(interpreter.state.code, Value::new_primitive(PW_Print));

			push(interpreter.state.code, Value::new_number({
				.pos = reinterpret_cast<idx_t>(s_end)
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
		pop(interpreter.state.code); // ? function
		pop(interpreter.state.code); // code length

		assert(false); // TODO: some sort of error or something?

		return {};
	}
}

void interpret_word_def(Interpreter &interpreter) {
	idx_t code_start = length(interpreter.state.code);
	size_t code_len = 0;

	const char *name = nullptr;
	size_t name_len = 0;
	const char *desc = nullptr;
	size_t desc_len = 0;

	interpreter.get_word();
	if (interpreter.curr_word.len == 0) {
		// TODO:
		// error_fun(":", "expected word name");
		interpreter.state.error = "Error: expected word name";
		interpreter.state.error_handled = false;
		return;
	} else {
		name = interpreter.curr_word.text;
		name_len = interpreter.curr_word.len;
		interpreter.curr_word.handled = true;
	}

	interpreter.get_word();
	if (interpreter.curr_word.len == 1 && interpreter.curr_word.text[0] == '(') {
		interpreter.curr_word.handled = true;

		interpreter.get_word();

		if (interpreter.curr_word.len == 0) {
			// TODO:
			// error_fun(":", "expected matching ) for start of description");
			interpreter.state.error = "Error: expected matching ) for start of description";
			interpreter.state.error_handled = false;
			return;
		} else {
			desc = interpreter.curr_word.text;
			interpreter.curr_word.handled = true;
		}

		while (true) {
			assert(interpreter.curr_word.len != 0);
			desc_len = interpreter.curr_word.text + interpreter.curr_word.len - desc;

			interpreter.get_word();

			if (interpreter.curr_word.len == 1 && interpreter.curr_word.text[0] == ')') {
				interpreter.curr_word.handled = true;
				break;
			} else if (interpreter.curr_word.len == 1 && interpreter.curr_word.text[0] == '(') {
				assert(interpreter.ignore_next()); // skip embedded comments
			} else if (interpreter.curr_word.len == 0) {
				// TODO:
				// error_fun(":", "expected matching ) for start of description");
				interpreter.state.error = "Error: expected matching ) for start of description";
				interpreter.state.error_handled = false;
				return;
			}

			interpreter.curr_word.handled = true;
		}
	}

	while (true) {
		interpreter.get_word();

		if (interpreter.curr_word.len == 1 && interpreter.curr_word.text[0] == ';') {
			interpreter.curr_word.handled = true;
			break;
		} else if (interpreter.curr_word.len == 0) {
			interpreter.state.error = "Error: unterminated word definition";
			interpreter.state.error_handled = false;
			break;
		}

		const auto compiled_len = interpreter.compile_next();
		if (has(compiled_len)) {
			code_len += get(compiled_len);
		} else {
			for (size_t i = 0; i < code_len; ++i) {
				pop(interpreter.state.code);
			}

			// TODO: Some sort of a proper error
			interpreter.state.error = "Error: undefined word";
			interpreter.state.error_handled = false;
			return;
		}
	}

	interpreter.state.define_word(name, name_len, desc, desc_len, code_start, code_len);
}

void ignore_word_def(Interpreter &interpreter) {
	while (true) {
		interpreter.get_word();

		if (interpreter.curr_word.len == 1 && interpreter.curr_word.text[0] == ';') {
			interpreter.curr_word.handled = true;
			break;
		} else if (interpreter.curr_word.len == 0) {
			interpreter.state.error = "Error: unterminated word definition";
			interpreter.state.error_handled = false;
			break;
		}

		interpreter.ignore_next();
	}
}

void interpret_rep_and(Interpreter &interpreter) {
	const size_t initial_size = length(interpreter.state.code);
	const idx_t code_pos = initial_size;
	const auto rep_len = interpreter.compile_next();

	if (has(rep_len)) {
		// TODO:
		// check_stack_len_ge("rep_and", 1);

		const size_t n = pop(interpreter.state.stack).pos;

		for (size_t i = 0; i < n; ++i) {
			Runner runner = { {
				.code = &interpreter.state.code[code_pos],
				.len = get(rep_len),
			}, interpreter.state };

			while (!interpreter.state.error && runner.curr.len > 0) {
				runner.run_next();
			}
		}

		while (length(interpreter.state.code) > initial_size) {
			pop(interpreter.state.code);
		}

		if (interpreter.state.error == nullptr) {
			// TODO:
			// check_stack_cap("rep_and", 1);

			push(interpreter.state.stack, { .pos = n });
		}
	} else {
		// TODO:
		interpreter.state.error = "Error: invalid code after rep_and";
		interpreter.state.error_handled = false;
		return;
	}
}

void ignore_rep_and(Interpreter &interpreter) {
	assert(interpreter.ignore_next()); // TODO: some sort of error or something?
}

extern RawFunction rep_and;
maybe_t<size_t> compile_rep_and(Interpreter &interpreter) {
	// TODO:
	// check_code_len ...

	push(interpreter.state.code, Value::new_number({ .pos = 0 }));
	Value &skip_len = interpreter.state.code[length(interpreter.state.code)-1];

	push(interpreter.state.code, Value::new_function_ptr(&rep_and));

	const auto next_len = interpreter.compile_next();
	if (has(next_len)) {
		skip_len.number.pos = get(next_len);

		return get(next_len)+2;
	} else {
		pop(interpreter.state.code); // rep_and function
		pop(interpreter.state.code); // code length

		assert(false); // TODO: some sort of error or something?

		return {};
	}
}

void interpret_rep(Interpreter &interpreter) {
	interpret_rep_and(interpreter);
	if (interpreter.state.error == nullptr) {
		pop(interpreter.state.stack);
	}
}

maybe_t<size_t> compile_rep(Interpreter &interpreter) {
	const auto rep_and_size = compile_rep_and(interpreter);
	if (has(rep_and_size)) {
		assert(get(interpreter.compile_primitive_idx(PW_Drop)) == 1);

		return get(rep_and_size) + 1;
	} else {
		// TODO: error?
		return {};
	}
}

void interpret_block(Interpreter &interpreter) {
	while (true) {
		interpreter.get_word();

		if (interpreter.curr_word.len == 1 && interpreter.curr_word.text[0] == ']') {
			interpreter.curr_word.handled = true;
			break;
		} else if (interpreter.curr_word.len == 0) {
			interpreter.state.error = "Error: unclosed block";
			interpreter.state.error_handled = false;
			break;
		}

		if (!interpreter.run_next()) {
			// TODO: proper error handling
			interpreter.state.error = "Error: unrecognised word while parsing block";
			interpreter.state.error_handled = false;
			break;
		}
	}
}

void ignore_block(Interpreter &interpreter) {
	while (true) {
		interpreter.get_word();

		if (interpreter.curr_word.len == 1 && interpreter.curr_word.text[0] == ']') {
			interpreter.curr_word.handled = true;
			break;
		} else if (interpreter.curr_word.len == 0) {
			interpreter.state.error = "Error: unclosed block";
			interpreter.state.error_handled = false;
			break;
		}

		if (!interpreter.ignore_next()) {
			// TODO: proper error handling
			interpreter.state.error = "Error: unrecognised word while parsing block";
			interpreter.state.error_handled = false;
			break;
		}
	}
}

maybe_t<size_t> compile_block(Interpreter &interpreter) {
	const size_t initial_code_len = length(interpreter.state.code);

	size_t total_len = 0;

	while (true) {
		interpreter.get_word();

		if (interpreter.curr_word.len == 1 && interpreter.curr_word.text[0] == ']') {
			interpreter.curr_word.handled = true;
			break;
		} else if (interpreter.curr_word.len == 0) {
			interpreter.state.error = "Error: unclosed block";
			interpreter.state.error_handled = false;
			break;
		}

		const auto next_size = interpreter.compile_next();
		if (!has(next_size)) {
			// TODO: proper error handling
			if (interpreter.state.error) break;
			interpreter.state.error = "Error: unrecognised word while parsing block";
			interpreter.state.error_handled = false;
			break;
		} else {
			total_len += get(next_size);
		}
	}

	if (interpreter.state.error) {
		while (length(interpreter.state.code) > initial_code_len) {
			pop(interpreter.state.code);
		}

		return {};
	} else {
		return total_len;
	}
}

/*** SECTION: Raw function values ***/

RawFunction print_raw = { "<internal:print_raw>", [](Runner &runner) {
	// TODO:
	// check_stack_len_ge("<internal:print_raw>", 1);
	const char *str = reinterpret_cast<const char*>(pop(runner.state.stack).pos);
	writestring(str);
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
		assert(skip_len <= runner.curr.len);
		runner.curr.code += skip_len;
		runner.curr.len -= skip_len;
	}
} };

RawFunction rep_and = { "rep_and", [](Runner &runner) {
	// TODO:
	// check_stack_len_ge("rep_and", 2);

	const size_t rep_len = pop(runner.state.stack).pos;
	const Value *rep_until = runner.curr.code + rep_len;
	const size_t reps = pop(runner.state.stack).pos;

	const auto start_at = runner.curr;

	for (size_t i = 0; i < reps; ++i) {
		while (runner.curr.code < rep_until) {
			runner.run_next();
		}

		assert(runner.curr.code == rep_until);

		runner.curr = start_at;
	}

	assert(rep_len <= runner.curr.len);
	runner.curr.code += rep_len;
	runner.curr.len -= rep_len;

	// TODO:
	// check_stack_cap("rep_and", 1);

	push(runner.state.stack, { .pos = reps });
} };

}

/*** SECTION: Syntax Array ***/

const Syntax syntax[SC_COUNT] = {
	/* VALUES */
	[SC_String] = {
		"\"", "-- ... n ; pushes a string to the top of the stack (data then length)",
		interpret_string, ignore_string, compile_string
	},
	[SC_Hex] = {
		"hex", "-- a ; interprets next word as hex number and pushes it",
		interpret_hex, ignore_hex, compile_hex,
	},
	[SC_ShortStr] = {
		"'", "-- a ; interprets next word as short (<= 4 long) string and pushes it",
		interpret_short_str, ignore_short_str, compile_short_str,
	},

	/* DOCUMENTATION / HELP / INSPECTION */
	[SC_Help] = {
		"help", "-- ; prints help text for the next word",
		interpret_help, ignore_help, compile_help,
	},
	[SC_Def] = {
		"def", "-- ; prints the definition of a given word",
		interpret_def, ignore_def, compile_def,
	},

	/* INTERNALS / SYNTAX */
	[SC_Comment] = {
		"(", "-- ; begins a ( comment )", ignore_comment, ignore_comment,
		[](Interpreter &interpreter) -> maybe_t<size_t> {
			ignore_comment(interpreter);
			return 0;
		}
	},
	[SC_Rec] = {
		"rec", "-- ; recurses (runs the current word from the start)",
		[](Interpreter &interpreter) {
			// TODO:
			// error_fun("rec", "rec is only valid when defining a word (inside : ; )");
			interpreter.state.error = "rec is only valid when defining a word";
			interpreter.state.error_handled = false;
		}, [](Interpreter&) {}, compile_rec,
	},
	[SC_Ret] = {
		"ret", "-- ; returns (exits the current word early)",
		[](Interpreter &interpreter) {
			// TODO:
			// error_fun("ret", "ret is only valid when defining a word (inside : ; )");
			interpreter.state.error = "ret is only valid when defining a word";
			interpreter.state.error_handled = false;
		}, [](Interpreter&) {}, compile_ret,
	},
	[SC_Skip] = {
		"?", "a -- ; only executes the next word if the stack top is nonzero",
		interpret_skip, ignore_skip, compile_skip,
	},
	[SC_WordDef] = {
		":", "-- ; begins a user-supplied word definition",
		interpret_word_def, ignore_word_def,
		[] (Interpreter &interpreter) -> maybe_t<size_t> {
			// TODO:
			// error_fun(":", "new words may only be defined while interpreting a line");
			interpreter.state.error = ": is not valid inside a word definition";
			interpreter.state.error_handled = false;

			return {};
		},
	},
	[SC_RepAnd] = {
		"rep_and", "n -- ??? n ; repeat the next word n times, and push n to the stack",
		interpret_rep_and, ignore_rep_and, compile_rep_and,
	},
	[SC_Rep] = {
		"rep", "n -- ??? ; repeat the next word n times",
		interpret_rep, ignore_rep_and, compile_rep,
	},
	[SC_Block] = {
		"[", "-- ; begins a [ block ], treated as one unit by ?, rep, etc",
		interpret_block, ignore_block, compile_block,
	},
};

}
