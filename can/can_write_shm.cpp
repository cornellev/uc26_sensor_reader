#include <cstdint>
#include <cstdio>
#include <cstring>


constexpr uint8_t rx_fifo_num = 1;
constexpr uint8_t tx_fifo_num = 2;
constexpr uint8_t filter_num = 0;

constexpr uint32_t PI_ID = 1;
constexpr uint32_t BOARD_ID = 2;

int main() {
    mcp.init();
    // bring up chip

    mcp.setTranMode(TMODE_MCP_NORM);
    // normal transmit behavior

    mcp.setContMode(CMODE_MCP_CFD_NORM);
    // enable CAN FD normal mode

    mcp.initTXQ(
        PL_SIZE_MCP_12, /* payload size of 12 bytes */
        8, /* queue can hold eight messages */
        2, /* prioority of 2 = remote board */
        TXRET_MCP_UNLIM, /* retry unlimited number of times */
        nullptr, /* nooooooooooooooooooooo flags */
        0
    );
    // initialize TX queue (where outgoing messages go)

    mcp.initGPFIFO(
        rx_fifo_num,
        FIFO_MODE_MCP_RX,
        PL_SIZE_MCP_12,
        8, /* fifo can store eight messages */
        1, /* priority of 1 = pi */
        TXRET_MCP_UNLIM, /* retransmit until success */
        nullptr, /* no flags */
        0
    );
    // create an RX FIFO that can hold incoming messages (size = 12-byte payload for now)

    mcp.initFilter(filter_num, rx_fifo_num, PI_ID);
    // only accept messages addressed to PI_ID, route them into rx_fifo_num

    for(;;) {
        uint32_t request_payload[1] = {0x00};
        // 1-byte null payload just to indicate that we're requesting sensor data

        uint32_t request_msg[8 + 1]; /* 8 byte header + 1 byte payload */

        create_message_obj(
            request_msg,
            request_payload,
            CAN_FD_BASE_MCP,
            PL_SIZE_MCP_1,
            BOARD_ID,
            1 /* bit rate switching enabled */
        );
        // build the CAN FD message

        mcp.pushTXFIFO(tx_fifo_num, request_msg, header_size + payload_size);
        // write the message into MCP internal RAM via TX FIFO

        mcp.reqSendTXFIFO(tx_fifo_num);
        // tells MCP to actually transmit the message on the CAN bus

        // Wait for response
        bool ok = false;

        while (!ok) {
            ok = mcp.popRXFIFO(rx_fifo_num, response_msg, 8 + 12);
            // tries to read a message from RX FIFO

            if(ok) {

                payload = header_size + response_msg;

                ts     = bytes_to_u32(payload[0..3]);
                value1 = bytes_to_float(payload[4..7]);
                value2 = bytes_to_float(payload[8..11]);

                break;
            }
        }
    }

    return 0;
}
