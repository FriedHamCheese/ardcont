#include "OutputDevicesInterface.hpp"
#include "OutputDeviceData.hpp"
#include "audeng_wrapper.hpp"

#include <chrono>
#include <iostream>

OutputDevicesInterface::OutputDevicesInterface(
	const PaDeviceIndex audience_output_device_index, 
	const PaDeviceIndex monitor_output_device_index,
	GlobalStates& global_states
)
:	global_states(global_states)
{
	const PaTime audience_output_latency = Pa_GetDeviceInfo(audience_output_device_index)->defaultLowOutputLatency;
	const PaTime monitor_output_latency = Pa_GetDeviceInfo(monitor_output_device_index)->defaultLowOutputLatency;
	PaTime agreed_latency = audience_output_latency;
	if(audience_output_latency < monitor_output_latency) 
		agreed_latency = monitor_output_latency;
	
	const OutputDeviceData monitor_data(global_states, monitor_output_device_index, agreed_latency, true);
	this->monitor_output_device_thread = std::thread(run_audio_engine, monitor_data);
	this->monitor_output_device_thread.detach();

	const bool has_one_output_device = audience_output_device_index == monitor_output_device_index;
	if(not has_one_output_device){
		const OutputDeviceData audience_data(global_states, audience_output_device_index, agreed_latency, false);
		this->audience_output_device_thread = std::thread(run_audio_engine, audience_data);
		this->audience_output_device_thread.detach();
	}
}

void OutputDevicesInterface::run() noexcept{
	while(this->global_states.requested_exit.load() == false){
		try{
			const std::unique_ptr<AudioTrack>& left_deck = this->global_states.audio_tracks[0];
			const std::unique_ptr<AudioTrack>& right_deck = this->global_states.audio_tracks[1];
			
			const uint8_t monitor_device_status = global_states.monitor_output_device_status.load();
			switch(monitor_device_status){
				case AudioTrackAccess_ReadingBlocked:{
					if(not left_deck->get_sample_access_mutex().try_lock())
						break;
					if(not right_deck->get_sample_access_mutex().try_lock()){
						left_deck->get_sample_access_mutex().unlock();
						break;
					}
		
					global_states.audience_output_device_status = AudioTrackAccess_ReadyForReading;
					global_states.monitor_output_device_status = AudioTrackAccess_ReadyForReading;
					break;
				}
				case AudioTrackAccess_ReadyForReading:
				break;
				
				case AudioTrackAccess_BeingRead:
				break;
				
				case AudioTrackAccess_FinishedReading:{
					if(global_states.audience_output_device_status.load() == AudioTrackAccess_BeingRead)
						break;

					left_deck->get_sample_access_mutex().unlock();
					right_deck->get_sample_access_mutex().unlock();
					
					try{
						std::thread left_deck_load_thread = std::thread(left_deck->load_samples, left_deck.get());
						std::thread right_deck_load_thread = std::thread(right_deck->load_samples, right_deck.get());
						left_deck_load_thread.detach();
						right_deck_load_thread.detach();
						
						global_states.audience_output_device_status = AudioTrackAccess_ReadingBlocked;
						global_states.monitor_output_device_status = AudioTrackAccess_ReadingBlocked;
					}
					catch(const std::system_error& thread_spawn_failed){
						std::cerr << "OutputDevicesInterface: run(): Failed to spawn a thread for loading samples.\n" << thread_spawn_failed.what() << '\n';
						std::cout << ": " << std::flush;
					}
					break;
				}
				default:
				std::cerr << "OutputDevicesInterface: run(): Invalid monitor device status.\n";
				std::cout << ": " << std::flush;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		catch(const std::exception& excp){
			std::cerr << "OutputDevicesInterface: run(): Uncaught exception: " << excp.what() << '\n';
			std::cerr << "Continuing...\n";
			std::cout << ": " << std::flush;
		}
		catch(...){
			std::cerr << "OutputDevicesInterface: run(): Uncaught throw.\n";
			std::cerr << "Continuing...\n";
			std::cout << ": " << std::flush;
		}
	}
}