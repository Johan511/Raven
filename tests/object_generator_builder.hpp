///////////////////////////////////////////////////////////
#include "strong_types.hpp"
#include <chrono>
#include <thread>
#include <vector>
///////////////////////////////////////////////////////////
#include <data_manager.hpp>
#include <utilities.hpp>
///////////////////////////////////////////////////////////

class ObjectGeneratorFactory
{
    // returns answer in bytes
    static std::uint64_t
    get_size_of_object(std::uint64_t bitRate, std::chrono::milliseconds msBetweenObjects)
    {
        std::uint64_t bytesPerSecond = bitRate / 8;
        std::uint64_t bytesPerObject = bytesPerSecond * msBetweenObjects.count() / 1000;
        return bytesPerObject;
    }

    static std::uint64_t get_current_ms_timestamp()
    {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
        .count();
    }

    static std::string generate_object(std::uint64_t objectSize)
    {
        std::uint64_t currTime = get_current_ms_timestamp();
        std::string object(objectSize, 0);
        std::memcpy(object.data(), &currTime, sizeof(currTime));
        return object;
    }

    rvn::DataManager& dataManager_;

public:
    enum class LayerGranularity
    {
        TrackGranularity, // each layer represents a different track
        GroupGranularity  // each layer represents a different group
    };
    using enum LayerGranularity;

    ObjectGeneratorFactory(rvn::DataManager& dataManager)
    : dataManager_(dataManager)
    {
    }

    // Bitrate among layers changes with a 2^layerId multiplier
    std::vector<std::thread> create(LayerGranularity layerGranularity,
                                    std::uint8_t numLayers,
                                    std::uint64_t numObjectsPerLayer,
                                    std::chrono::milliseconds msBetweenObjects,
                                    std::uint64_t baseBitRate)
    {
        std::vector<std::thread> objectGenerators(numLayers);

        static const std::vector<std::string> trackNamespace{ "namespace1", "namespace2",
                                                              "namespace3" };

        const auto trackGranularityGeneratorTask =
        [this, baseBitRate, numObjectsPerLayer, msBetweenObjects](std::uint64_t layerIdx)
        {
            std::uint64_t bitRate = baseBitRate << layerIdx;
            std::uint64_t objectSize = get_size_of_object(bitRate, msBetweenObjects);
            rvn::PublisherPriority priority(5 - layerIdx);
            if (layerIdx == 0)
                priority = rvn::PublisherPriority(-1);

            auto trackHandle =
            dataManager_
            .add_track_identifier(trackNamespace, std::to_string(layerIdx),
                                  priority, std::nullopt)
            .lock();


            for (std::uint64_t objectId = 0; objectId < numObjectsPerLayer; ++objectId)
            {
                std::string object = generate_object(objectSize);
                trackHandle->add_object(rvn::GroupId(layerIdx),
                                        rvn::ObjectId(objectId), std::move(object));
                std::this_thread::sleep_for(msBetweenObjects);
            }
        };

        for (std::size_t layerIdx = 0; auto& th : objectGenerators)
        {
            if (layerGranularity == LayerGranularity::TrackGranularity)
                th = std::thread(trackGranularityGeneratorTask, layerIdx);
            else if (layerGranularity == LayerGranularity::GroupGranularity)
            {
                assert(false);
                exit(1);
            }
            else
                throw std::invalid_argument("Invalid layer granularity");
            ++layerIdx;
        }

        return objectGenerators;
    }
};
