/**********************************************************************
 * This file is used for testing random stuff without running the
 * whole of SatDump, which comes in handy for debugging individual
 * elements before putting them all together in modules...
 *
 * If you are an user, ignore this file which will not be built by
 * default, and if you're a developper in need of doing stuff here...
 * Go ahead!
 *
 * Don't judge the code you might see in there! :)
 **********************************************************************/

#include "logger.h"

#include "products/image_products.h"

image::Image<uint16_t> calibrate_channel(satdump::ImageProducts &img_pro, int channel, double range_min, double range_max)
{
    bool albedo = img_pro.get_calibration_type(channel) == img_pro.CALIB_REFLECTANCE;

    image::Image<uint16_t> output(img_pro.images[channel].image.width(), img_pro.images[channel].image.height(), 1);

    auto dec_start = std::chrono::system_clock::now();

    logger->info("Start calib...");

    // #pragma omp parallel for
    for (int x = 0; x < img_pro.images[channel].image.width(); x++)
    {
        for (int y = 0; y < img_pro.images[channel].image.height(); y++)
        {
            if (albedo)
            {
                output[y * output.width() + x] = output.clamp(img_pro.get_calibrated_value(channel, x, y) * 0.01 * 65535);
            }
            else
            {
                // logger->critical(img_pro.get_calibrated_value(channel, x, y));
                double val = (img_pro.get_calibrated_value(channel, x, y) - range_min) / abs(range_min - range_max);
                output[y * output.width() + x] = output.clamp(val * 65535);
            }
        }
    }

    auto dec_time = (std::chrono::system_clock::now() - dec_start);
    double time_sec = dec_time.count() / 1e9;

    logger->info("Took {:f} to calibrate!", time_sec);

    return output;
}

int main(int /*argc*/, char *argv[])
{
    initLogger();

    satdump::ImageProducts img_pro;
    img_pro.load(argv[1]);
    img_pro.init_calibration();

    auto output = calibrate_channel(img_pro, 0, 0, 1);
    output = calibrate_channel(img_pro, 1, 0, 1);
    output = calibrate_channel(img_pro, 2, 0, 1);
    output = calibrate_channel(img_pro, 3, 0, 1);
    output = calibrate_channel(img_pro, 4, 0, 1);
    output = calibrate_channel(img_pro, 5, 0, 1);
    // auto output = calibrate_channel(img_pro, 5, 0, 120);
    // auto output = calibrate_channel(img_pro, 4, 0, 120);

    output.save_png(argv[2]);
}
