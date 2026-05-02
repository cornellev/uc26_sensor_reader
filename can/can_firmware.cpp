#include <cstdint>
#include <cstdio>
#include <cstring>

#include "RP2040_MCP251863_Driver/include/mcp251863.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

constexpr uint8_t rx_fifo_num = 1;
constexpr uint8_t tx_fifo_num = 2;
constexpr uint8_t filter_num = 0;

constexpr uint32_t PI_ID = 1;
constexpr uint32_t BOARD_ID = 2;

constexpr uint32_t header_size = 8;

// 12 bytes bc i think ethan said so
constexpr uint32_t request_payload_size = 12;
constexpr uint32_t request_msg_size = header_size + request_payload_size;

// 4 byte timestamp + 4 byte float + 4 byte float
constexpr uint32_t response_payload_size = 12;
constexpr uint32_t response_msg_size = header_size + response_payload_size;

// spi pin configs
// TODO: adjust to match actual pcb
constexpr uint SPI_SCK_PIN = 18;
constexpr uint SPI_MOSI_PIN = 19;
constexpr uint SPI_MISO_PIN = 16;
constexpr uint MCP_CS_PIN = 17;
constexpr uint MCP_STBY_PIN = 15;

int main() {
    stdio_init_all();

    // spi hardware init
    spi_init(spi0, MCP251863_BAUD_RATE);
    gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_MISO_PIN, GPIO_FUNC_SPI);

    // cs and stby as output gpio
    gpio_init(MCP_CS_PIN);
    gpio_set_dir(MCP_CS_PIN, GPIO_OUT);
    gpio_put(MCP_CS_PIN, 1);

    gpio_init(MCP_STBY_PIN);
    gpio_set_dir(MCP_STBY_PIN, GPIO_OUT);

    // constructs mcp. need to find actual values to use for constructor
    MCP251863 mcp{spi0, MCP_CS_PIN, MCP_STBY_PIN};

    // bring up chip
    mcp.init();

    // normal transmit behavior
    mcp.setTranMode(TMODE_MCP_NORM);

    // enable CAN FD normal mode
    mcp.setContMode(CMODE_MCP_CFD_NORM);

    // initialize TX queue (where outgoing messages go)
    mcp.initGPFIFO(
        tx_fifo_num,
        FIFO_MODE_MCP_TX,
        PL_SIZE_MCP_12,  /* 12-byte request payload */
        8,               /* queue can hold eight messages */
        2,               /* priority of 2 = remote board */
        TXRET_MCP_UNLIM, /* retry unlimited number of times */
        nullptr,         /* no flags */
        0);

    // create an RX FIFO that can hold incoming messages (size = 12-byte payload
    // for now)
    mcp.initGPFIFO(
        rx_fifo_num,
        FIFO_MODE_MCP_RX,
        PL_SIZE_MCP_12,
        8,               /* fifo can store eight messages */
        1,               /* priority of 1 = pi */
        TXRET_MCP_UNLIM, /* retransmit until success */
        nullptr,         /* no flags */
        0);

    // only accept messages addressed to PI_ID, route them into rx_fifo_num
    mcp.initFilter(filter_num, rx_fifo_num, PI_ID);

    for (;;) {
        // zeroed payload just to indicate that we're requesting sensor data
        uint8_t request_payload[request_payload_size] = {0x00};

        uint8_t request_msg[request_msg_size];

        // build the CAN FD message
        create_message_obj(
            request_msg,
            request_payload,
            CAN_FD_BASE_MCP,
            PL_SIZE_MCP_12,
            BOARD_ID,
            1 /* bit rate switching enabled */
        );

        // write the message into MCP internal RAM via TX FIFO
        mcp.pushTXFIFO(tx_fifo_num, request_msg, request_msg_size);

        // tells MCP to actually transmit the message on the CAN bus
        mcp.reqSendTXFIFO(tx_fifo_num);

        // Wait for response
        uint8_t response_msg[response_msg_size];
        bool ok = false;

        while (!ok) {
            // tries to read a message from RX FIFO
            ok = mcp.popRXFIFO(rx_fifo_num, response_msg, response_msg_size);

            if (ok) {
                uint8_t* response_payload = response_msg + header_size;

                uint32_t ts;
                float value1, value2;
                memcpy(&ts, response_payload + 0, sizeof(uint32_t));
                memcpy(&value1, response_payload + 4, sizeof(float));
                memcpy(&value2, response_payload + 8, sizeof(float));

                printf("ts=%lu v1=%f v2=%f\n", (unsigned long)ts, (double)value1, (double)value2);
            }
        }
    }

    return 0;
}
