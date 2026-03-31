
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio.h"
#include "pico/time.h"
#include "canbus.h"
#include "obd.h"
#include "mcp251x/mcp251x.h"

static MCP251x *can_device = NULL;
static spi_instance_t *can_spi_bus = NULL;

static float mass_air_flow = 0.0f;
static float engine_speed = 0.0f;
static float vehicle_speed = 0.0f;

float mpg = 0.0f;

void update_mpg()
{
    /// Calculate fuel mass flow from mass air flow, assume a stoichiometric air-fuel ratio of 14.7:1.
    float fuel_mass_flow = mass_air_flow / 14.7f; // in g/s

    /// Convert fuel mass flow to volume flow, assume a fuel density of 850 g/L.
    float fuel_volume_flow = fuel_mass_flow / 850.0f; // in L/s

    /// Convert to gallons per hour (1 L/s = 0.219969 GPH, 3600s in an hour).
    fuel_volume_flow = (fuel_volume_flow * 0.219969f) * 3600; // in GPH

    /// Calulate miles per hour from vehicle speed (kph).
    float miles_per_hour = vehicle_speed * 0.621371f;

    /// Finally, calculate miles per gallon. MPG = MPH / GPH.
    mpg = miles_per_hour / fuel_volume_flow;
    printf("MPG: %.3f\n", mpg);
}

void core1_entry()
{
    printf("Core 1 started, initialising CAN controller...\n");
    can_spi_bus = spi_instance_init(SPI_HW_1, 10, 11, 12, 13, false, 10000000);
    can_device = mcp251x_get_device();

    mcp251x_config info;
    info.model = MODEL_MCP2515;
    info.crystal_oscillator = MCP251x_12MHZ;
    info.spi_dev = can_spi_bus;

    // Initialise the chip with config.
    if (mcp251x_init(can_device, &info) != MCP251x_ERR_SUCCESS)
        return;

    // Change bitrate to 500 KBPS
    mcp251x_set_bitrate(can_device, CAN_BITRATE_500KBPS);

    int index = 0;

    mcp251x_set_mode(can_device, MCP251x_MODE_NORMAL);

    while (1)
    {
        can_frame rxframe;

        static uint32_t counter = 0;
        // if (++counter % 1000 == 0)
        // printf("Alive %lu\n", counter);

        if (mcp251x_read_frame(can_device, &rxframe) == MCP251x_ERR_SUCCESS)
        {
            // printf("R %03x\n", rxframe.id);
            if ((rxframe.id & 0x700) == 0x700) // OBD-II request frame, ignore.
            {
                printf("[%03x: %d] ", rxframe.id, rxframe.dlc);
                for (int i = 0; i < rxframe.dlc; i++)
                {
                    printf("%02x ", rxframe.data[i]);
                }
                printf("\n");

                if (rxframe.dlc >= 6 && rxframe.data[1] == (OBD_SERVICE_SHOW_CURRENT_DATA + 0x40))
                {
                    const uint8_t pid = rxframe.data[2];

                    if (pid == OBD_PIDS_MASS_AIR_FLOW_RATE)
                    {
                        mass_air_flow = ((rxframe.data[3] << 8) | rxframe.data[4]) / 100.0f;
                        printf("Mass Air Flow: %.2f g/s\n", mass_air_flow);
                        update_mpg();
                    }

                    if (pid == OBD_PIDS_ENGINE_SPEED)
                    {
                        engine_speed = ((rxframe.data[3] << 8) | rxframe.data[4]) / 4.0f;
                        printf("Engine Speed: %.2f RPM\n", engine_speed);
                        update_mpg();
                    }

                    if (pid == OBD_PIDS_VEHICLE_SPEED)
                    {
                        vehicle_speed = rxframe.data[3];
                        printf("Vehicle Speed: %0.2f kph\n", vehicle_speed);
                        update_mpg();
                    }

                    // if (pid == OBD_PIDS_SUPPORTED_21_TO_40 || pid == OBD_PIDS_SUPPORTED_41_TO_60)
                    // {
                    //     const uint8_t base_pid = (pid == OBD_PIDS_SUPPORTED_21_TO_40) ? 0x21 : 0x41;
                    //     uint32_t supported = *((uint32_t *)(rxframe.data + 3));
                    //     printf("Supported PID range %02X (%s): 0x%08X\n", pid,
                    //            (pid == OBD_PIDS_SUPPORTED_21_TO_40) ? "21-40" : "41-60",
                    //            supported);
                    //     printf("\n");
                    // }
                }
            }
        }

        if (index == 1)
        {
            // Send OBD-II request for supported PIDs.
            can_frame txframe;
            txframe.id = OBD_QUERY_CANID; // OBD-II request frame.
            txframe.dlc = 8;
            txframe.data[0] = 2; // Number of additional bytes (service + PID).
            txframe.data[1] = OBD_SERVICE_SHOW_CURRENT_DATA;
            txframe.data[2] = OBD_PIDS_VEHICLE_SPEED;
            memset(txframe.data + 3, 0xCC, 5); // Pad remaining bytes with 0.

            mcp251x_error err = mcp251x_send_frame(can_device, &txframe);
            // printf("Sending request..\n");
            if (err != MCP251x_ERR_SUCCESS)
            {
                printf("Failed to send OBD-II VS request frame: %d\n", err);
            }
        }

        if (index == 4)
        {
            // Send OBD-II request for supported PIDs.
            can_frame txframe;
            txframe.id = OBD_QUERY_CANID; // OBD-II request frame.
            txframe.dlc = 8;
            txframe.data[0] = 2; // Number of additional bytes (service + PID).
            txframe.data[1] = OBD_SERVICE_SHOW_CURRENT_DATA;
            txframe.data[2] = OBD_PIDS_ENGINE_SPEED;
            memset(txframe.data + 3, 0xCC, 5); // Pad remaining bytes with 0.

            mcp251x_error err = mcp251x_send_frame(can_device, &txframe);
            // printf("Sending request..\n");
            if (err != MCP251x_ERR_SUCCESS)
            {
                printf("Failed to send OBD-II ES request frame: %d\n", err);
            }
        }

        if (index == 7)
        {
            // Send OBD-II request for supported PIDs.
            can_frame txframe;
            txframe.id = OBD_QUERY_CANID; // OBD-II request frame.
            txframe.dlc = 8;
            txframe.data[0] = 2; // Number of additional bytes (service + PID).
            txframe.data[1] = OBD_SERVICE_SHOW_CURRENT_DATA;
            txframe.data[2] = OBD_PIDS_MASS_AIR_FLOW_RATE;
            memset(txframe.data + 3, 0xCC, 5); // Pad remaining bytes with 0.

            mcp251x_error err = mcp251x_send_frame(can_device, &txframe);
            // printf("Sending request..\n");
            if (err != MCP251x_ERR_SUCCESS)
            {
                printf("Failed to send OBD-II MAF request frame: %d\n", err);
            }
        }

        index++;
        index %= 10;
        sleep_ms(10);
    }

    // Clean up memory, ensure can controller is destroyed first.
    mcp251x_destroy(can_device);
    spi_instance_destroy(can_spi_bus);
}