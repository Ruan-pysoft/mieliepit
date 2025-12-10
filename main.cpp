#include <string>
#include <iostream>

#include "mieliepit.hpp"

using namespace mieliepit;

bool should_quit = false;

Primitive primitives[] = {
	{ "quit", "", [](ProgramState &state) {
		should_quit = true;
	} },
	{ "+", "some description", [](ProgramState &state) {
		const number_t a = pop(state.stack);
		const number_t b = pop(state.stack);
		const number_t res = {
			.pos = a.pos + b.pos
		};
		state.stack.push_back(res);
	} },
	{ ".", "some description", [](ProgramState &state) {
		if (state.stack.size() == 0) {
			std::cout << "empty." << std::endl;
		} else {
			for (const auto number : state.stack) {
				std::cout << number.sign << ' ';
			}
			std::cout << std::endl;
		}
	} },
	{ "drop", "", [](ProgramState &state) {
		pop(state.stack);
	} },
	{ "dup", "", [](ProgramState &state) {
		state.stack.push_back(*state.stack.rbegin());
	} },
};

int main() {
	mieliepit::ProgramState state {
		.stack = {},
		.code = {},
		.error = nullptr,

		.words = {},
		.primitives = primitives,
		.primitives_len = sizeof(primitives)/sizeof(*primitives),
		.syntax = syntax,
		.syntax_len = syntax_len,
	};

	mieliepit::Interpreter interpreter {
		.line = nullptr,
		.len = 0,
		.action = Interpreter::Run,
		.curr_word = {},
		.state = state,
	};

	while (!should_quit) {
		std::cout << "> ";
		std::string line;
		std::getline(std::cin, line);

		interpreter.line = line.c_str();
		interpreter.len = line.size();

		while (!state.error && interpreter.len > 0) {
			interpreter.advance();
		}
	}
}
