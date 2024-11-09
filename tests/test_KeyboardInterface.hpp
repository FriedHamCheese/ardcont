#ifndef TEST_KEYBOARDINTERFACE_HPP
#define TEST_KEYBOARDINTERFACE_HPP

#include <gtest/gtest.h>
#include "KeyboardInterface.hpp"
#include "GlobalStates.hpp"

class KeyboardListenerTest : public testing::Test{
	protected:
	void SetUp() override{
		global_states.reset_to_initial_state();
		test_flags.clear_all();
	}
	
	GlobalStates global_states;
	KeyboardListenerTestingFlags test_flags;
	KeyboardMock keyboard_input_mock;
};

TEST_F(KeyboardListenerTest, EmptylineIsInvalidCommand){
	keyboard_input_mock.setline("");
	keyboard_listener(keyboard_input_mock, global_states, &test_flags);
	EXPECT_EQ(test_flags.invalid_command, true);
}

TEST_F(KeyboardListenerTest, JumbledTextIsInvalidCommand){
	keyboard_input_mock.setline("edidjoiwjido3jp");
	keyboard_listener(keyboard_input_mock, global_states, &test_flags);	
	EXPECT_EQ(test_flags.invalid_command, true);
}

TEST_F(KeyboardListenerTest, qMeansQuitting){
	keyboard_input_mock.setline("q");
	keyboard_listener(keyboard_input_mock, global_states, &test_flags);	
	EXPECT_EQ(global_states.requested_exit, true);
}

TEST_F(KeyboardListenerTest, qDiscardsArgumentsAndQuits){
	keyboard_input_mock.setline("q 12345 kdkokd");
	keyboard_listener(keyboard_input_mock, global_states, &test_flags);	
	EXPECT_EQ(global_states.requested_exit, true);
}

TEST_F(KeyboardListenerTest, lWithoutArgumentsComplains){
	keyboard_input_mock.setline("l");
	keyboard_listener(keyboard_input_mock, global_states, &test_flags);	
	EXPECT_EQ(test_flags.l_command_not_enough_arguments, true);
}

TEST_F(KeyboardListenerTest, lWithoutFilenameComplains){
	keyboard_input_mock.setline("l 123");
	keyboard_listener(keyboard_input_mock, global_states, &test_flags);	
	EXPECT_EQ(test_flags.l_command_not_enough_arguments, true);
}

TEST_F(KeyboardListenerTest, lTrackIDNotANumber){
	keyboard_input_mock.setline("l abc ./cookies.wav");
	keyboard_listener(keyboard_input_mock, global_states, &test_flags);	
	EXPECT_EQ(test_flags.l_command_id_not_a_number, true);
}

TEST_F(KeyboardListenerTest, lTrackIDOutOfRange){
	keyboard_input_mock.setline("l 5 ./cookies.wav");
	keyboard_listener(keyboard_input_mock, global_states, &test_flags);	
	EXPECT_EQ(test_flags.l_command_id_out_of_range, true);
}

TEST_F(KeyboardListenerTest, lNegativeTrackIDOutOfRange){
	keyboard_input_mock.setline("l -35 ./cookies.wav");
	keyboard_listener(keyboard_input_mock, global_states, &test_flags);	
	EXPECT_EQ(test_flags.l_command_id_out_of_range, true);
}

#endif