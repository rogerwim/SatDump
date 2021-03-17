#include "module_noaa_dsb_demod.h"
#include "modules/common/dsp/lib/fir_gen.h"
#include "logger.h"
#include "modules/common/manchester.h"
#include "imgui/imgui.h"
#include <volk/volk.h>

// Return filesize
size_t getFilesize(std::string filepath);

namespace noaa
{
    NOAADSBDemodModule::NOAADSBDemodModule(std::string input_file, std::string output_file_hint, std::map<std::string, std::string> parameters) : ProcessingModule(input_file, output_file_hint, parameters),
                                                                                                                                                  d_samplerate(std::stoi(parameters["samplerate"])),
                                                                                                                                                  d_buffer_size(std::stoi(parameters["buffer_size"]))
    {
        // Init DSP blocks
        file_source = std::make_shared<dsp::FileSourceBlock>(d_input_file, dsp::BasebandTypeFromString(parameters["baseband_format"]), d_buffer_size);
        agc = std::make_shared<dsp::AGCBlock>(file_source->output_stream, 1e-3f, 1.0f, 1.0f, 65536);
        pll = std::make_shared<dsp::BPSKCarrierPLLBlock>(agc->output_stream, 0.01f, powf(0.01, 2) / 4.0f, 3.0f * M_PI * 100e3 / (float)d_samplerate);
        rrc = std::make_shared<dsp::FFFIRBlock>(pll->output_stream, 1, dsp::firgen::root_raised_cosine(1, (float)d_samplerate / 2.0f, (float)8320, 0.5, 1023));
        rec = std::make_shared<dsp::FFMMClockRecoveryBlock>(rrc->output_stream, ((float)d_samplerate / (float)8320) / 2.0f, powf(0.01, 2) / 4.0f, 0.5f, 0.01, 100e-6);
        rep = std::make_shared<RepackBitsByte>();
        def = std::make_shared<DSBDeframer>();

        // Buffers
        bits_buffer = new uint8_t[d_buffer_size * 10];
        bytes_buffer = new uint8_t[d_buffer_size * 10];
        manchester_buffer = new uint8_t[d_buffer_size * 10];
    }

    NOAADSBDemodModule::~NOAADSBDemodModule()
    {
        delete[] bits_buffer;
        delete[] bytes_buffer;
        delete[] manchester_buffer;
    }

    void NOAADSBDemodModule::process()
    {
        if (input_data_type == DATA_FILE)
            filesize = file_source->getFilesize();
        else
            filesize = 0;

        //if (input_data_type == DATA_FILE)
        //    data_in = std::ifstream(d_input_file, std::ios::binary);

        data_out = std::ofstream(d_output_file_hint + ".tip", std::ios::binary);
        d_output_files.push_back(d_output_file_hint + ".tip");

        logger->info("Using input baseband " + d_input_file);
        logger->info("Demodulating to " + d_output_file_hint + ".tip");
        logger->info("Buffer size : " + std::to_string(d_buffer_size));

        time_t lastTime = 0;

        // Start
        file_source->start();
        agc->start();
        pll->start();
        rrc->start();
        rec->start();

        int dat_size = 0, bytes_out = 0, num_byte = 0;
        while (/*input_data_type == DATA_STREAM ? input_active.load() : */ !file_source->eof())
        {
            dat_size = rec->output_stream->read();

            if (dat_size <= 0)
                continue;

            volk_32f_binary_slicer_8i((int8_t *)bits_buffer, rec->output_stream->readBuf, dat_size);

            rec->output_stream->flush();

            bytes_out = rep->work(bits_buffer, dat_size, bytes_buffer);

            defra_buf.insert(defra_buf.end(), &bytes_buffer[0], &bytes_buffer[bytes_out]);
            num_byte = defra_buf.size() - defra_buf.size() % 2;

            manchesterDecoder(&defra_buf.data()[0], num_byte, manchester_buffer);

            std::vector<std::array<uint8_t, FRAME_SIZE>> frames = def->work(manchester_buffer, num_byte / 2);

            // Count frames
            frame_count += frames.size();

            // Erase used up elements
            defra_buf.erase(defra_buf.begin(), defra_buf.begin() + num_byte);

            // Write to file
            if (frames.size() > 0)
            {
                for (std::array<uint8_t, FRAME_SIZE> frm : frames)
                {
                    data_out.write((char *)&frm[0], FRAME_SIZE);
                }
            }

            progress = file_source->getPosition();
            if (time(NULL) % 10 == 0 && lastTime != time(NULL))
            {
                lastTime = time(NULL);
                std::string deframer_state = def->getState() == 0 ? "NOSYNC" : (def->getState() == 2 || def->getState() == 6 ? "SYNCING" : "SYNCED");
                logger->info("Progress " + std::to_string(round(((float)progress / (float)filesize) * 1000.0f) / 10.0f) + "%, Deframer : " + deframer_state + ", Frames : " + std::to_string(frame_count));
            }
        }

        logger->info("Demodulation finished");

        // Stop
        file_source->stop();
        agc->stop();
        pll->stop();
        rrc->stop();
        rec->stop();

        data_out.close();
    }

    const ImColor colorNosync = ImColor::HSV(0 / 360.0, 1, 1, 1.0);
    const ImColor colorSyncing = ImColor::HSV(39.0 / 360.0, 0.93, 1, 1.0);
    const ImColor colorSynced = ImColor::HSV(113.0 / 360.0, 1, 1, 1.0);

    void NOAADSBDemodModule::drawUI()
    {
        ImGui::Begin("NOAA DSB Demodulator", NULL, NOWINDOW_FLAGS);

        ImGui::BeginGroup();
        // Constellation
        {
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            draw_list->AddRectFilled(ImGui::GetCursorScreenPos(),
                                     ImVec2(ImGui::GetCursorScreenPos().x + 200, ImGui::GetCursorScreenPos().y + 200),
                                     ImColor::HSV(0, 0, 0));

            for (int i = 0; i < 2048; i++)
            {
                draw_list->AddCircleFilled(ImVec2(ImGui::GetCursorScreenPos().x + (int)(100 + rec->output_stream->readBuf[i] * 90) % 200,
                                                  ImGui::GetCursorScreenPos().y + (int)(100 + rng.gasdev() * 15) % 200),
                                           2,
                                           ImColor::HSV(113.0 / 360.0, 1, 1, 1.0));
            }

            ImGui::Dummy(ImVec2(200 + 3, 200 + 3));
        }
        ImGui::EndGroup();

        ImGui::SameLine();

        ImGui::BeginGroup();
        {
            ImGui::Button("Deframer", {200, 20});
            {
                ImGui::Text("State : ");

                ImGui::SameLine();

                if (def->getState() == 0)
                    ImGui::TextColored(colorNosync, "NOSYNC");
                else if (def->getState() == 2 || def->getState() == 6)
                    ImGui::TextColored(colorSyncing, "SYNCING");
                else
                    ImGui::TextColored(colorSynced, "SYNCED");

                ImGui::Text("Frames : ");

                ImGui::SameLine();

                ImGui::TextColored(ImColor::HSV(113.0 / 360.0, 1, 1, 1.0), std::to_string(frame_count).c_str());
            }
        }
        ImGui::EndGroup();

        ImGui::ProgressBar((float)progress / (float)filesize, ImVec2(ImGui::GetWindowWidth() - 10, 20));

        ImGui::End();
    }

    std::string NOAADSBDemodModule::getID()
    {
        return "noaa_dsb_demod";
    }

    std::vector<std::string> NOAADSBDemodModule::getParameters()
    {
        return {"samplerate", "buffer_size", "baseband_format"};
    }

    std::shared_ptr<ProcessingModule> NOAADSBDemodModule::getInstance(std::string input_file, std::string output_file_hint, std::map<std::string, std::string> parameters)
    {
        return std::make_shared<NOAADSBDemodModule>(input_file, output_file_hint, parameters);
    }
} // namespace noaa