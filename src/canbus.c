/**
 * @file canbus.c
 * @author Leo Walker
 * @brief Core 1 code for handling CAN bus communication and updating mpg global value.
 * @ref canbus.h & obd.h for CAN bus and OBD-II specific definitions and constants.
 *
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * This software is written under the GNU GENERAL PUBLIC LICENSE Version 3.
 * and a copy of the license can be found in the root of this project as 'LICENSE'.
 * @ref LICENSE for more details.
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

/// C Headers.
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/// Pico SDK Headers.
#include "pico/stdlib.h" // For standard library functions and types.
#include "pico/stdio.h"  // For printf debugging.
#include "pico/time.h"   // For timing the main loop.

/// CAN Library Header.
#include "mcp251x/mcp251x.h" // For interfacing with the MCP251x external CAN controller chip via the RP2350's SPI hardware.

/// Project Headers.
#include "canbus.h" // For exposing the core 1 entry point and global variables to other translation units.
#include "obd.h"    // For OBD-II specific constants such as PIDs and CAN IDs.

/// CAN bus specific variables and objects.
static MCP251x *can_device = NULL;
static spi_instance_t *can_spi_bus = NULL;

static float mass_air_flow = 0.0f;
static float vehicle_speed = 0.0f;

/// Global miles per gallon variable. This value is updated and displayed to the screen.
float mpg = 0.0f;

/**
 * @brief Function to calculate miles per gallon from the current mass air flow and vehicle speed.
 * Called on the recption of new data.
 */
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
}

/**
 * @brief Core 1 entry point, handles the CAN bus communication on the OBD-II interface.
 * Requests and recieved OBD-II frames and calculates MPG based on mass air flow and vehicle speed.
 * Main loop runs at 200hz to recieve all the frames on the bus without loss.
 * OBD-II PID requests are sent every 50ms / 20hz, alternating between vehicle speed and mass air flow rate and recalculating MPG.
 */
void core1_entry()
{
    printf("Core 1 started, initialising CAN controller...\n");

    // Initialise SPI bus and CAN bus controller (mcp2515).
    can_spi_bus = spi_instance_init(SPI_HW_1, 10, 11, 12, 13, false, 10000000);
    can_device = mcp251x_get_device();

    /// Configure the CAN controller with the correct hardware settings, and initialise it.
    mcp251x_config info;
    info.model = MODEL_MCP2515;
    info.crystal_oscillator = MCP251x_12MHZ;
    info.spi_dev = can_spi_bus;

    // Initialise the chip with config.
    if (mcp251x_init(can_device, &info) != MCP251x_ERR_SUCCESS)
        return;

    // Change bitrate to 500 KBPS, as this is the standard bitrate for OBD-II on CAN.
    mcp251x_set_bitrate(can_device, CAN_BITRATE_500KBPS);

    // Loop counter to time sending OBD-II requests.
    int index = 0;

    // Set CAN controller to normal mode to enable communication on the bus and begin receiving frames.
    mcp251x_set_mode(can_device, MCP251x_MODE_NORMAL);

    /**
     * Core 1 main loop.
     * 200hz
     */
    while (1)
    {
        // Record start of loop timestamp.
        uint64_t start_loop_time = time_us_64();

        // Object to store recieved CAN message.
        can_frame rxframe;

        // Read any recieved frames from the CAN controller, and if it's a OBD-II response then process the data.
        if (mcp251x_read_frame(can_device, &rxframe) == MCP251x_ERR_SUCCESS)
        {
            // Check if the CAN message is an OBD-II response.
            if ((rxframe.id & 0x700) == 0x700)
            {
                // Check data structure and length are as expected.
                if (rxframe.dlc >= 6 && rxframe.data[1] == (OBD_SERVICE_SHOW_CURRENT_DATA + 0x40))
                {
                    // Extract PID from response.
                    const uint8_t pid = rxframe.data[2];

                    // If the PID is mass air flow, extract the data and re-calculate MPG.
                    if (pid == OBD_PIDS_MASS_AIR_FLOW_RATE)
                    {
                        mass_air_flow = ((rxframe.data[3] << 8) | rxframe.data[4]) / 100.0f;
                        printf("Mass Air Flow: %.2f g/s\n", mass_air_flow);
                        update_mpg();
                    }

                    // If the PID is vehicle speed, extract the data and re-calculate MPG.
                    if (pid == OBD_PIDS_VEHICLE_SPEED)
                    {
                        vehicle_speed = rxframe.data[3];
                        printf("Vehicle Speed: %0.2f kph\n", vehicle_speed);
                        update_mpg();
                    }
                }
            }
        }

        // If index is 0 (10hz), send OBD-II request for Vehicle Speed.
        if (index == 0)
        {
            // Object to store OBD-II request frame.
            can_frame txframe;
            txframe.id = OBD_QUERY_CANID;                    // Set CAN Id as OBD-II request frame. (0x7DF)
            txframe.dlc = 8;                                 // OBD requests are always 8 bytes long, unused bytes are padded with 0xCC.
            txframe.data[0] = 2;                             // Number of additional bytes (service + PID).
            txframe.data[1] = OBD_SERVICE_SHOW_CURRENT_DATA; // OBD service for requesting live data.
            txframe.data[2] = OBD_PIDS_VEHICLE_SPEED;        // Set PID for vehicle speed in request.
            memset(txframe.data + 3, 0xCC, 5);               // Pad remaining bytes with 0xCC per recommendations.

            // Transmit OBD-II request frame.
            mcp251x_error err = mcp251x_send_frame(can_device, &txframe);

            // Log any errors in transmission.
            if (err != MCP251x_ERR_SUCCESS)
            {
                printf("Failed to send OBD-II VS request frame: %d\n", err);
            }
        }

        // If index is 10 (10hz), send OBD-II request for Mass Air Flow.
        if (index == 10)
        {
            // Object to store OBD-II request frame.
            can_frame txframe;
            txframe.id = OBD_QUERY_CANID;                    // Set CAN Id as OBD-II request frame. (0x7DF)
            txframe.dlc = 8;                                 // OBD requests are always 8 bytes long, unused bytes are padded with 0xCC.
            txframe.data[0] = 2;                             // Number of additional bytes (service + PID).
            txframe.data[1] = OBD_SERVICE_SHOW_CURRENT_DATA; // OBD service for requesting live data.
            txframe.data[2] = OBD_PIDS_MASS_AIR_FLOW_RATE;   // Set PID for mass air flow in request.
            memset(txframe.data + 3, 0xCC, 5);               // Pad remaining bytes with 0xCC per recommendations.

            // Transmit OBD-II request frame.
            mcp251x_error err = mcp251x_send_frame(can_device, &txframe);

            // Log any errors in transmission.
            if (err != MCP251x_ERR_SUCCESS)
            {
                printf("Failed to send OBD-II MAF request frame: %d\n", err);
            }
        }

        // Increment loop counter and wrap around at 20 (50ms / 20hz).
        index++;
        index %= 20;

        // Calculate time elapsed in loop and sleep to enforce 200hz loop rate.
        uint64_t end_loop_time = time_us_64();
        uint64_t loop_time = end_loop_time - start_loop_time;
        if (loop_time < 5000) // 5000 microseconds = 5ms
        {
            sleep_us(5000 - loop_time);
        }
    }
    // If loop is ever exited, clean up resources before terminating core 1.
    // Clean up memory, ensure can controller is destroyed first as it is dependent on the SPI instance.
    mcp251x_destroy(can_device);
    spi_instance_destroy(can_spi_bus);
}

// EOF