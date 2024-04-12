#include "cmd_spi_driver.h"
#include "hal/spi.h"

//---- CONSTANTS -------------------------------------------------------------------------------------------------------------------------------------------------

#define LOAD_CMD_FIFO_SIZE 64      // RX buffer size for incomming communication from the interface panel

//---- STRUCTS ---------------------------------------------------------------------------------------------------------------------------------------------------

// RX circular buffer data structure
typedef struct {

    volatile uint32_t data[LOAD_CMD_FIFO_SIZE];     // data (23:8) and checksum (7:0)
    volatile uint32_t head;
    volatile uint32_t tail;
    volatile bool     is_full;

} load_cmd_fifo_t;

//---- ENUMERATIONS ----------------------------------------------------------------------------------------------------------------------------------------------

// frame parsing state machine states
typedef enum {

    STATE_WAITING_FOR_FRAME_SYNC,   // address reception will start after receiving a frame synchronization byte (0xff)
    STATE_RECEIVING_ADDRESS,        // after the address and r/!w bit is read the state machine will continue into receiving or transmitting based on the r/!w bit

    STATE_RECEIVING_DATA_HIGH,
    STATE_RECEIVING_DATA_LOW,
    STATE_RECEIVING_CHECKSUM,       // after the checksum is received, it is pushed onto the RX fifo and state machine is reset

    STATE_TRANSMITTING_DATA_HIGH,
    STATE_TRANSMITTING_DATA_LOW,
    STATE_TRANSMITTING_CHECKSUM     // after the checksum is sent, the state machine is reset

} load_cmd_state_machine_t;

//---- PRIVATE DATA ----------------------------------------------------------------------------------------------------------------------------------------------

static load_cmd_fifo_t cmd_fifo;                        // fifo for buffering the incomming write commands
static uint32_t cmd_register[CMD_REGISTER_COUNT];       // register table from which the data for read commands is read

//---- INTERNAL FUNCTIONS ----------------------------------------------------------------------------------------------------------------------------------------

// calculates the checksum of a data frame
static inline uint8_t __calculate_checksum(uint8_t address, uint16_t data) {

    return (~((uint8_t)(address) ^ (uint8_t)(data & 0xff) ^ (uint8_t)((data >> 8) & 0xff)));
}

//---- FUNCTIONS -------------------------------------------------------------------------------------------------------------------------------------------------

// initializes the CMD interface slave SPI driver
void cmd_driver_init(void) {

    rcc_enable_peripheral_clock(CMD_SPI_SS_GPIO_CLOCK);
    rcc_enable_peripheral_clock(CMD_SPI_SCK_GPIO_CLOCK);
    rcc_enable_peripheral_clock(CMD_SPI_MOSI_GPIO_CLOCK);
    rcc_enable_peripheral_clock(CMD_SPI_MISO_GPIO_CLOCK);
    rcc_enable_peripheral_clock(CMD_SPI_CLOCK);

    gpio_set_mode(CMD_SPI_SS_GPIO, GPIO_MODE_ALTERNATE_FUNCTION);
    gpio_set_mode(CMD_SPI_SCK_GPIO, GPIO_MODE_ALTERNATE_FUNCTION);
    gpio_set_mode(CMD_SPI_MOSI_GPIO, GPIO_MODE_ALTERNATE_FUNCTION);
    gpio_set_mode(CMD_SPI_MISO_GPIO, GPIO_MODE_ALTERNATE_FUNCTION);

    gpio_set_alternate_function(CMD_SPI_SS_GPIO, CMD_SPI_GPIO_AF);
    gpio_set_alternate_function(CMD_SPI_SCK_GPIO, CMD_SPI_GPIO_AF);
    gpio_set_alternate_function(CMD_SPI_MOSI_GPIO, CMD_SPI_GPIO_AF);
    gpio_set_alternate_function(CMD_SPI_MISO_GPIO, CMD_SPI_GPIO_AF);

    CMD_SPI->CR1  = 0;
    CMD_SPI->CR2  = SPI_CR2_RXNEIE;     // enable the RX buffer not empty interrupt
    CMD_SPI->CR1 |= SPI_CR1_SPE;        // SPI enable

    // reset the write command fifo
    cmd_fifo.tail = cmd_fifo.head;
    cmd_fifo.is_full = false;

    // reset the registers
    for (int i = 0; i < CMD_REGISTER_COUNT; i++) cmd_register[i] = 0;

    NVIC_SetPriority(CMD_SPI_IRQ, 1);
    NVIC_EnableIRQ(CMD_SPI_IRQ);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// return true if there are unread write commands available in the RX buffer
bool cmd_has_data(void) {

    return ((cmd_fifo.head != cmd_fifo.tail) || cmd_fifo.is_full);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// reads one message from the write command buffer; callee should read the cmd_has_data() flag first; returns true if the checksum of the message is correct
bool cmd_read(uint8_t *address, uint16_t *data) {

    if ((cmd_fifo.head == cmd_fifo.tail) && !cmd_fifo.is_full) return false;        // return if the fifo is empty

    // pop the data frame from the fifo
    uint32_t data_frame = cmd_fifo.data[cmd_fifo.tail++];
    if (cmd_fifo.tail == LOAD_CMD_FIFO_SIZE) cmd_fifo.tail = 0;
    cmd_fifo.is_full = false;

    *address = data_frame >> 24;
    *data = (data_frame >> 8) & 0xffff;
    uint8_t  checksum = data_frame & 0xff;
    
    return (checksum == __calculate_checksum(*address, *data));     // calculate the correct checksum and compare
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// writes data to the specified register; if the interface receives a read command on this address, this value will be transmitted to the master
void cmd_write(uint8_t address, uint16_t data) {

    if (!cmd_address_valid(address)) return;

    uint8_t checksum = __calculate_checksum(address | (CMD_READ_BIT >> 24), data);
    cmd_register[address] = ((data & 0xffff) << 8) | checksum;
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// sets a bit in a load register
void cmd_set_bit(uint8_t address, uint16_t mask) {

    if (!cmd_address_valid(address)) return;

    uint16_t data = cmd_register[address] >> 8;
    data |= mask;

    cmd_write(address, data);
}

//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

// clears a bit in a load register
void cmd_clear_bit(uint8_t address, uint16_t mask) {

    if (!cmd_address_valid(address)) return;

    uint16_t data = cmd_register[address] >> 8;
    data &= ~mask;

    cmd_write(address, data);
}

//---- IRQ HANDLERS ----------------------------------------------------------------------------------------------------------------------------------------------

void CMD_SPI_IRQ_HANDLER(void) {

    static load_cmd_state_machine_t frame_state = STATE_WAITING_FOR_FRAME_SYNC;     // tracking the current part of a frame being sent or received
    static uint32_t data_frame = 0;                                                 // for assembling the received or transmitted data frame

    //---- READING WRITE COMMANDS FROM MASTER --------------------------------------------------------------------------------------------------------------------

    if (spi_rx_not_empty(CMD_SPI)) {

        uint8_t received_byte = spi_read(CMD_SPI);

        switch (frame_state) {

            case STATE_WAITING_FOR_FRAME_SYNC:

                // start receiving address if the frame synchronization byte is correct
                if (received_byte == CMD_FRAME_SYNC_BYTE) frame_state = STATE_RECEIVING_ADDRESS;
                break;

            case STATE_RECEIVING_ADDRESS:

                data_frame = received_byte << 24;   // read the address and read/!write bit
                
                if (data_frame & CMD_READ_BIT) {
                    
                    frame_state = STATE_TRANSMITTING_DATA_HIGH;

                    // if the register address is valid, load the register into the data frame; else send zeroes
                    if (cmd_address_valid(received_byte & 0x7f)) data_frame |= cmd_register[received_byte & 0x7f];
 
                    spi_write(CMD_SPI, (data_frame >> 16) & 0xff);  // send data high byte
                    set_bits(CMD_SPI->CR2, SPI_CR2_TXEIE);          // enable the TX buffer empty interrupt

                } else frame_state = STATE_RECEIVING_DATA_HIGH;

                break;

            case STATE_RECEIVING_DATA_HIGH:

                data_frame |= received_byte << 16;   // read data high byte
                frame_state = STATE_RECEIVING_DATA_LOW;
                break;

            case STATE_RECEIVING_DATA_LOW:

                data_frame |= received_byte << 8;    // read data low byte
                frame_state = STATE_RECEIVING_CHECKSUM;
                break;

            case STATE_RECEIVING_CHECKSUM:

                data_frame |= received_byte;         // read the checksum
                frame_state = STATE_WAITING_FOR_FRAME_SYNC;

                if (!cmd_fifo.is_full) {        // the data frame is complete, push it onto the fifo

                    cmd_fifo.data[cmd_fifo.head++] = data_frame;

                    if (cmd_fifo.head == LOAD_CMD_FIFO_SIZE) cmd_fifo.head = 0;
                    if (cmd_fifo.head == cmd_fifo.tail) cmd_fifo.is_full = true;
                }

                break;

            default:
                break;
        }
    }

    //---- RESPONDING TO READ COMMANDS ---------------------------------------------------------------------------------------------------------------------------

    if (bit_is_set(CMD_SPI->CR2, SPI_CR2_TXEIE) && spi_tx_empty(CMD_SPI)) {

        switch (frame_state) {

            case STATE_TRANSMITTING_DATA_HIGH:

                spi_write(CMD_SPI, (data_frame >> 8) & 0xff);      // send data low byte
                frame_state = STATE_TRANSMITTING_DATA_LOW;
                break;

            case STATE_TRANSMITTING_DATA_LOW:

                spi_write(CMD_SPI, data_frame & 0xff);      // send checksum
                frame_state = STATE_TRANSMITTING_CHECKSUM;
                break;

            case STATE_TRANSMITTING_CHECKSUM:
            default:

                spi_write(CMD_SPI, 0x00);
                clear_bits(CMD_SPI->CR2, SPI_CR2_TXEIE);    // disable the TX buffer empty interrupt
                frame_state = STATE_WAITING_FOR_FRAME_SYNC; // reset the state machine
                break;
        }
    }

    //------------------------------------------------------------------------------------------------------------------------------------------------------------
}

//----------------------------------------------------------------------------------------------------------------------------------------------------------------
