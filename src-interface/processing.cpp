#include "logger.h"
#include "core/module.h"
#include "core/pipeline.h"
#include <filesystem>
#include "nlohmann/json.hpp"
#include <fstream>
#include "processing.h"
#include "main_ui.h"

namespace processing
{
    void process(std::string downlink_pipeline,
                 std::string input_level,
                 std::string input_file,
                 std::string output_file,
                 nlohmann::json parameters)
    {
        isProcessing = true;

        logger->info("Starting processing pipeline " + downlink_pipeline + "...");
        logger->debug("Input file (" + input_level + ") : " + input_file);
        logger->debug("Output file : " + output_file);

        if (!std::filesystem::exists(output_file))
            std::filesystem::create_directories(output_file);

        // Get pipeline
        std::optional<Pipeline> pipeline = getPipelineFromName(downlink_pipeline);

        if (pipeline.has_value())
            pipeline.value().run(input_file, output_file, parameters, input_level, true, uiCallList, uiCallListMutex);
        else
            logger->critical("Pipeline " + downlink_pipeline + " does not exist!");

        logger->info("Done! Goodbye");
        isProcessing = false;
    }
}