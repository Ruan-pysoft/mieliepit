#include <string>
#include <iostream>

#include "mieliepit.hpp"

using namespace mieliepit;

bool should_quit = false;

void mieliepit::quit_primitive_fn(ProgramState &) {
	should_quit = true;
}

void mieliepit::guide_primitive_fn(ProgramState &) {
	std::cout << guide_text;
}

void interpret_str(Interpreter &interpreter, const std::string str, bool silent = false) {
	interpreter.state.error = nullptr;
	interpreter.state.error_handled = false;

	interpreter.line = str.c_str();
	interpreter.len = str.size();
	interpreter.curr_word = {};

	while (!interpreter.state.error && interpreter.len > 0) {
		interpreter.run_next();
	}

	if (interpreter.state.error) {
		if (!interpreter.state.error_handled) {
			std::cout << '\n' << interpreter.state.error << std::endl;
		}
		if (interpreter.curr_word.len == 0) {
			std::cout << "@ end of line" << std::endl;
		} else {
			std::cout << "@ word starting at " << (interpreter.curr_word.text - str.c_str()) << ": ";
			for (size_t i = 0; i < interpreter.curr_word.len; ++i) {
				std::cout << interpreter.curr_word.text[i];
			}
			std::cout << std::endl;
		}

		interpreter.state.error_handled = true;
	} else if (!silent) {
		std::cout << std::endl;
	}
}

int main() {
	ProgramState state {
		primitives, PW_COUNT,
		syntax, SC_COUNT,
	};

	Interpreter interpreter {
		.line = nullptr,
		.len = 0,
		.curr_word = {},
		.state = state,
	};

	interpret_str(interpreter, ": - ( a b -- a-b ) not inc + ;", true);
	interpret_str(interpreter, ": neg ( a -- -a ) 0 swap - ;", true);

	interpret_str(interpreter, ": *_under ( a b -- a a*b ) swap dup rot * ;", true);
	interpret_str(interpreter, ": ^ ( a b -- a^b ; a to the power b ) 1 swap rep *_under swap drop ;", true);

	interpret_str(interpreter, ": != ( a b -- a!=b ) = not ;", true);
	interpret_str(interpreter, ": <= ( a b -- a<=b ) dup rot dup rot < unrot = or ;", true);
	interpret_str(interpreter, ": >= ( a b -- a>=b ) < not ;", true);
	interpret_str(interpreter, ": > ( a b -- a>=b ) <= not ;", true);

	interpret_str(interpreter, ": truthy? ( a -- a!=false ) false != ;", true);

	interpret_str(interpreter, ": show_top ( a -- a ; prints the topmost stack element ) dup print ;", true);
	interpret_str(interpreter, ": clear ( ... - ; clears the stack ) stack_len 0 = ? ret drop rec ;", true);

	while (!should_quit) {
		std::cout << "> ";
		std::string line;
		std::getline(std::cin, line);

		if (std::cin.eof() || std::cin.fail()) {
			break;
		}

		interpret_str(interpreter, line);
	}
}
