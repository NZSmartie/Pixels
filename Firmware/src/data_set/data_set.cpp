#include "data_set.h"
#include "data_set_data.h"
#include "utils/utils.h"
#include "drivers_nrf/flash.h"
#include "drivers_nrf/scheduler.h"
#include "drivers_nrf/timers.h"
#include "drivers_nrf/watchdog.h"
#include "modules/accelerometer.h"
#include "config/board_config.h"
#include "config/settings.h"
#include "bluetooth/bluetooth_messages.h"
#include "bluetooth/bluetooth_message_service.h"
#include "bluetooth/bulk_data_transfer.h"
#include "malloc.h"
#include "assert.h"
#include "nrf_log.h"
#include "nrf_delay.h"


using namespace Utils;
using namespace DriversNRF;
using namespace Bluetooth;
using namespace Config;
using namespace Modules;
using namespace Animations;
using namespace Behaviors;


#define MAX_ACC_CLIENTS 2

namespace DataSet
{
	uint32_t computeDataSetSize();
	uint32_t computeDataSetHash();

	uint32_t getDataSetAddress() {
		return SettingsManager::getSettingsEndAddress();
	}

	uint32_t getDataSetDataAddress() {
		return getDataSetAddress() + sizeof(Data);
	}

	DelegateArray<ProgrammingEventMethod, MAX_ACC_CLIENTS> programmingClients;

	// The animation set always points at a specific address in memory
	Data const * data = nullptr;

	// A simple hash value of the dataset data
	uint32_t size = 0;
	uint32_t hash = 0;

	uint32_t dataSize() {
		return size;
	}

	uint32_t dataHash() {
		return hash;
	}

	void init(DataSetWrittenCallback callback) {

		static DataSetWrittenCallback _callback; // Don't initialize this static inline because it would only do it on first call!
		_callback = callback;
		data = (Data const *)getDataSetAddress();

		// This gets called after the animation set has been initialized
		auto finishInit = [] (bool result) {

				size = computeDataSetSize();
				hash = computeDataSetHash();

				MessageService::RegisterMessageHandler(Message::MessageType_TransferAnimSet, nullptr, ReceiveDataSetHandler);
				NRF_LOG_INFO("DataSet initialized, size=0x%x, hash=0x%08x", size, hash);
				auto callBackCopy = _callback;
				_callback = nullptr;
				if (callBackCopy != nullptr) {
					callBackCopy(result);
				}
			};

		//ProgramDefaultDataSet();
		if (!CheckValid()) {
			NRF_LOG_INFO("Animation Set not valid, programming default");
			ProgramDefaultDataSet(finishInit);
		} else {
			finishInit(true);
		}
		//printAnimationInfo();
	}

	/// <summary>
	/// Checks whether the animation set in flash is valid or garbage data
	/// </summary>
	bool CheckValid() {
		return data->headMarker == ANIMATION_SET_VALID_KEY &&
			data->version == ANIMATION_SET_VERSION &&
			data->tailMarker == ANIMATION_SET_VALID_KEY;
	}

	uint32_t getPaletteColor(uint16_t colorIndex) {
		if (colorIndex < (data->paletteSize / 3)) {
			return toColor(
					data->palette[colorIndex * 3 + 0],
					data->palette[colorIndex * 3 + 1],
					data->palette[colorIndex * 3 + 2]);
		} else {
			return 0xFFFFFFFF;
		}
	}

	uint16_t getPaletteSize() {
		assert(CheckValid());
		return data->paletteSize;
	}

	const RGBKeyframe& getRGBKeyframe(uint16_t keyFrameIndex) {
		assert(CheckValid() && keyFrameIndex < data->rgbKeyFrameCount);
		return data->rgbKeyframes[keyFrameIndex];
	}

	uint16_t getRGBKeyframeCount() {
		assert(CheckValid());
		return data->rgbKeyFrameCount;
	}

	const RGBTrack& getRGBTrack(uint16_t trackIndex) {
		assert(CheckValid() && trackIndex < data->rgbTrackCount);
		return data->rgbTracks[trackIndex];
	}

	RGBTrack const * const getRGBTracks(uint16_t tracksStartIndex) {
		assert(CheckValid() && tracksStartIndex < data->rgbTrackCount);
		return &(data->rgbTracks[tracksStartIndex]);
	}

	uint16_t getRGBTrackCount() {
		assert(CheckValid());
		return data->rgbTrackCount;
	}

	const RGBTrack& getHeatTrack() {
		return data->rgbTracks[data->heatTrackIndex];
	}

	const Keyframe& getKeyframe(uint16_t keyFrameIndex) {
		assert(CheckValid() && keyFrameIndex < data->keyFrameCount);
		return data->keyframes[keyFrameIndex];
	}

	uint16_t getKeyframeCount() {
		assert(CheckValid());
		return data->keyFrameCount;
	}

	const Track& getTrack(uint16_t trackIndex) {
		assert(CheckValid() && trackIndex < data->trackCount);
		return data->tracks[trackIndex];
	}

	Track const * const getTracks(uint16_t tracksStartIndex) {
		assert(CheckValid() && tracksStartIndex < data->trackCount);
		return &(data->tracks[tracksStartIndex]);
	}

	uint16_t getTrackCount() {
		assert(CheckValid());
		return data->trackCount;
	}

	const Animation* getAnimation(int animationIndex) {
		// Grab the preset data
		uint32_t animationAddress = (uint32_t)(const void*)data->animations + data->animationOffsets[animationIndex];
		return (const Animation *)animationAddress;
	}

	uint16_t getAnimationCount() {
		assert(CheckValid());
		return data->animationCount;
	}

	const Condition* getCondition(int conditionIndex) {
		assert(CheckValid());
		uint32_t conditionAddress = (uint32_t)(const void*)data->conditions + data->conditionsOffsets[conditionIndex];
		return (const Condition*)conditionAddress;
	}

	uint16_t getConditionCount() {
		assert(CheckValid());
		return data->conditionCount;
	}

	const Action* getAction(int actionIndex) {
		assert(CheckValid());
		uint32_t actionAddress = (uint32_t)(const void*)data->actions + data->actionsOffsets[actionIndex];
		return (const Action*)actionAddress;
	}

	uint16_t getActionCount() {
		assert(CheckValid());
		return data->actionCount;
	}

	const Rule* getRule(int ruleIndex) {
		assert(CheckValid());
		return &data->rules[ruleIndex];
	}

	uint16_t getRuleCount() {
		assert(CheckValid());
		return data->ruleCount;
	}

	// Behaviors
	const Behavior* getBehavior(int behaviorIndex) {
		assert(CheckValid());
		return &data->behaviors[behaviorIndex];
	}

	uint16_t getBehaviorCount() {
		assert(CheckValid());
		return data->behaviorsCount;
	}


	struct DataAndBufferSize
	{
		Data newData;
		uint32_t bufferSize;
	};
	DataAndBufferSize* dabs;

	void beginProgramming() {
		// Notify clients
		for (int i = 0; i < programmingClients.Count(); ++i)
		{
			programmingClients[i].handler(programmingClients[i].token, ProgrammingEventType_Begin);
		}
		// Allocate new dataset object
		dabs = (DataAndBufferSize*)malloc(sizeof(DataAndBufferSize));
	}

	void finishProgramming() {
		free(dabs);
		size = computeDataSetSize();
		hash = computeDataSetHash();
		NRF_LOG_INFO("Dataset size=0x%x, hash=0x%08x", size, hash);

		MessageService::SendMessage(Message::MessageType_TransferAnimSetFinished);

		// Notify clients
		for (int i = 0; i < programmingClients.Count(); ++i)
		{
			programmingClients[i].handler(programmingClients[i].token, ProgrammingEventType_End);
		}
	};

	void ReceiveDataSetHandler(void* context, const Message* msg) {

		NRF_LOG_INFO("Received Request to download new animation set");
		const MessageTransferAnimSet* message = (const MessageTransferAnimSet*)msg;

		beginProgramming();

		dabs->bufferSize =
			message->paletteSize * sizeof(uint8_t) +
			message->rgbKeyFrameCount * sizeof(RGBKeyframe) +
			message->rgbTrackCount * sizeof(RGBTrack) +
			message->keyFrameCount * sizeof(Keyframe) +
			message->trackCount * sizeof(Track) +
			Utils::roundUpTo4(sizeof(uint16_t) * message->animationCount) + // round up to multiple of 4
			message->animationSize +
			Utils::roundUpTo4(sizeof(uint16_t) * message->conditionCount) + // round up to multiple of 4
			message->conditionSize +
			Utils::roundUpTo4(sizeof(uint16_t) * message->actionCount) + // round up to multiple of 4
			message->actionSize +
			message->ruleCount * sizeof(Rule) +
			message->behaviorCount * sizeof(Behavior);

		NRF_LOG_DEBUG("Animation Data to be received:");
		NRF_LOG_DEBUG("Palette: %d * %d", message->paletteSize, sizeof(uint8_t));
		NRF_LOG_DEBUG("RGB Keyframes: %d * %d", message->rgbKeyFrameCount, sizeof(RGBKeyframe));
		NRF_LOG_DEBUG("RGB Tracks: %d * %d", message->rgbTrackCount, sizeof(RGBTrack));
		NRF_LOG_DEBUG("Keyframes: %d * %d", message->keyFrameCount, sizeof(Keyframe));
		NRF_LOG_DEBUG("Tracks: %d * %d", message->trackCount, sizeof(Track));
		NRF_LOG_DEBUG("Animation Offsets: %d * %d", message->animationCount, sizeof(uint16_t));
		NRF_LOG_DEBUG("Animations: %d", message->animationSize);
		NRF_LOG_DEBUG("Conditions Offsets: %d * %d", message->conditionCount, sizeof(uint16_t));
		NRF_LOG_DEBUG("Conditions: %d", message->conditionSize);
		NRF_LOG_DEBUG("Actions Offsets: %d * %d", message->actionCount, sizeof(uint16_t));
		NRF_LOG_DEBUG("Actions: %d", message->actionSize);
		NRF_LOG_DEBUG("Rules: %d * %d", message->ruleCount, sizeof(Rule));
		NRF_LOG_DEBUG("Behaviors: %d * %d", message->behaviorCount, sizeof(Behavior));

		uint32_t totalSize = dabs->bufferSize + sizeof(Data);
		uint32_t flashSize = Flash::getFlashByteSize(totalSize);
		uint32_t pageAddress = getDataSetAddress();
		uint32_t dataAddress = getDataSetDataAddress();
		uint32_t pageCount = Flash::bytesToPages(flashSize);

		NRF_LOG_DEBUG("totalSize: 0x%04x", totalSize);
		NRF_LOG_DEBUG("flashSize: 0x%04x", flashSize);
		NRF_LOG_DEBUG("pageAddress: 0x%08x", pageAddress);
		NRF_LOG_DEBUG("dataAddress: 0x%08x", dataAddress);
		NRF_LOG_DEBUG("pageCount: %d", pageCount);

		// Store the address and size
		NRF_LOG_DEBUG("Setting up pointers");
		dabs->newData.headMarker = ANIMATION_SET_VALID_KEY;
		dabs->newData.version = ANIMATION_SET_VERSION;

		uint32_t address = dataAddress;
		dabs->newData.palette = (const uint8_t*)address;
		dabs->newData.paletteSize = message->paletteSize;
		address += message->paletteSize * sizeof(uint8_t);

		dabs->newData.rgbKeyframes = (const RGBKeyframe*)address;
		dabs->newData.rgbKeyFrameCount = message->rgbKeyFrameCount;
		address += message->rgbKeyFrameCount * sizeof(RGBKeyframe);

		dabs->newData.rgbTracks = (const RGBTrack*)address;
		dabs->newData.rgbTrackCount = message->rgbTrackCount;
		address += message->rgbTrackCount * sizeof(RGBTrack);

		dabs->newData.keyframes = (const Keyframe*)address;
		dabs->newData.keyFrameCount = message->keyFrameCount;
		address += message->keyFrameCount * sizeof(Keyframe);

		dabs->newData.tracks = (const Track*)address;
		dabs->newData.trackCount = message->trackCount;
		address += message->trackCount * sizeof(Track);

		dabs->newData.animationOffsets = (const uint16_t*)address;
		dabs->newData.animationCount = message->animationCount;
		address += Utils::roundUpTo4(message->animationCount * sizeof(uint16_t)); // round to multiple of 4
		dabs->newData.animations = (const Animation*)address;
		dabs->newData.animationsSize = message->animationSize;
		address += message->animationSize;

		dabs->newData.conditionsOffsets = (const uint16_t*)address;
		dabs->newData.conditionCount = message->conditionCount;
		address += Utils::roundUpTo4(message->conditionCount * sizeof(uint16_t)); // round to multiple of 4
		dabs->newData.conditions = (const Condition*)address;
		dabs->newData.conditionsSize = message->conditionSize;
		address += message->conditionSize;

		dabs->newData.actionsOffsets = (const uint16_t*)address;
		dabs->newData.actionCount = message->actionCount;
		address += Utils::roundUpTo4(message->actionCount * sizeof(uint16_t)); // round to multiple of 4
		dabs->newData.actions = (const Action*)address;
		dabs->newData.actionsSize = message->actionSize;
		address += message->actionSize;

		dabs->newData.rules = (const Rule*)address;
		dabs->newData.ruleCount = message->ruleCount;
		address += message->ruleCount * sizeof(Rule);

		dabs->newData.behaviors = (const Behavior*)address;
		dabs->newData.behaviorsCount = message->behaviorCount;
		address += message->behaviorCount * sizeof(Behavior);

		dabs->newData.heatTrackIndex = message->heatTrackIndex;
		dabs->newData.tailMarker = ANIMATION_SET_VALID_KEY;

		
		// Start by erasing the flash
		Flash::erase(pageAddress, pageCount,
			[](bool result, uint32_t address, uint16_t data_size) {
				NRF_LOG_DEBUG("done Erasing %d page", data_size);

				if (result) {
					// Send Ack and receive data
					MessageService::SendMessage(Message::MessageType_TransferAnimSetAck);

					// Receive all the buffers directly to flash
					ReceiveBulkData::receiveToFlash(getDataSetDataAddress(), nullptr,
						[](void* context, bool success, uint16_t data_size) {
							NRF_LOG_DEBUG("Finished flashing animation data, flashing animation set itself");
							NRF_LOG_DEBUG("Buffer Size: 0x%04x, Programmed Size: 0x%04x", dabs->bufferSize, data_size);
							if (success) {
								// Program the animation set itself
								NRF_LOG_DEBUG("Writing set");

								Flash::write(getDataSetAddress(), &dabs->newData, sizeof(Data),
									[](bool result, uint32_t address, uint16_t data_size) {
										if (result) {
											NRF_LOG_INFO("Data Set written to flash!");

											if (!CheckValid()) {
												NRF_LOG_ERROR("Dateset is not valid, reprogramming defaults!");
												ProgramDefaultDataSet([](bool result) {
													finishProgramming();
												});
											} else {
												// All good!
												finishProgramming();
											}
										} else {
											NRF_LOG_ERROR("Error programming dataset to flash");
											finishProgramming();
										}
									}
								);
							} else {
								NRF_LOG_ERROR("Error transfering animation data");
								finishProgramming();
							}
						}
					);
				} else {
					NRF_LOG_ERROR("Error erasing flash");
					finishProgramming();
				}
			}
		);
	}

	void printAnimationInfo() {
		Timers::pause();
		NRF_LOG_INFO("Palette: %d * %d", data->paletteSize, sizeof(uint8_t));
		NRF_LOG_INFO("RGB Keyframes: %d * %d", data->rgbKeyFrameCount, sizeof(RGBKeyframe));
		NRF_LOG_INFO("RGB Tracks: %d * %d", data->rgbTrackCount, sizeof(RGBTrack));
		NRF_LOG_INFO("Keyframes: %d * %d", data->keyFrameCount, sizeof(Keyframe));
		NRF_LOG_INFO("Tracks: %d * %d", data->trackCount, sizeof(Track));
		NRF_LOG_INFO("Animation Offsets: %d * %d", data->animationCount, sizeof(uint16_t));
		NRF_LOG_INFO("Animations: %d", data->animationsSize);
		NRF_LOG_INFO("Conditions Offsets: %d * %d", data->conditionCount, sizeof(uint16_t));
		NRF_LOG_INFO("Conditions: %d", data->conditionsSize);
		NRF_LOG_INFO("Actions Offsets: %d * %d", data->actionCount, sizeof(uint16_t));
		NRF_LOG_INFO("Actions: %d", data->actionsSize);
		NRF_LOG_INFO("Rules: %d * %d", data->ruleCount, sizeof(Rule));
		NRF_LOG_INFO("Behaviors: %d * %d", data->behaviorsCount, sizeof(Behavior));
		Timers::resume();
	}

	uint32_t computeDataSetSize() {
		// Compute the size of the needed buffer to store all that data!
		return 
            data->paletteSize * sizeof(uint8_t) +
			data->rgbKeyFrameCount * sizeof(RGBKeyframe) +
			data->rgbTrackCount * sizeof(RGBTrack) +
			data->keyFrameCount * sizeof(Keyframe) +
			data->trackCount * sizeof(Track) +
			Utils::roundUpTo4(data->animationCount * sizeof(uint16_t)) + data->animationsSize +
            Utils::roundUpTo4(data->actionCount * sizeof(uint16_t)) + data->actionsSize +
            Utils::roundUpTo4(data->conditionCount * sizeof(uint16_t)) + data->conditionsSize + 
            data->ruleCount * sizeof(Rule) +
            data->behaviorsCount * sizeof(Behavior);
	}

	uint32_t computeDataSetHash() {
		return Utils::computeHash((const uint8_t*)getDataSetDataAddress(), size);
	}

	void hookProgrammingEvent(ProgrammingEventMethod client, void* param)
	{
		if (!programmingClients.Register(param, client))
		{
			NRF_LOG_ERROR("Too many hooks registered.");
		}
	}

	void unhookProgrammingEvent(ProgrammingEventMethod client)
	{
		programmingClients.UnregisterWithHandler(client);
	}

}
