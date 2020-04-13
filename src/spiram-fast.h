/*
 spiram-fast - Fast, hardcoded interface for SPI-based RAMs, allowing DIO
               mode to be used and speeding up individual SPI operations
	       significantly.

 Copyright (c) 2020 Earle F. Philhower, III   All rights reserved.

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

class ESP8266SPIRAM {
    private:
        typedef struct {
            volatile uint32_t spi_cmd; // The SPI can change this behind our backs, so volatile!
            uint32_t spi_addr;
            uint32_t spi_ctrl;
            uint32_t spi_ctrl1; // undocumented?  Not shown in the reg map
            uint32_t spi_rd_status;
            uint32_t spi_ctrl2;
            uint32_t spi_clock;
            uint32_t spi_user;
            uint32_t spi_user1;
            uint32_t spi_user2;
            uint32_t spi_wr_status;
            uint32_t spi_pin;
            uint32_t spi_slave;
            uint32_t spi_slave1;
            uint32_t spi_slave2;
            uint32_t spi_slave3;
            uint32_t spi_w[16]; // NOTE: You need a memory barrier before reading these after a read xaction
            uint32_t spi_ext3;
        } spi_regs;
        spi_regs *spi1 = (spi_regs*)&SPI1CMD;

        // The standard HSPI bus pins are used
        static constexpr uint8_t cs = 15;
        static constexpr uint8_t miso = 12;
        static constexpr uint8_t mosi = 13;
        static constexpr uint8_t sck = 14;

        uint32_t spi_clkval;

        typedef enum { sio = 0, dio = 1 } iotype;
        static constexpr iotype hspi_mode = dio;

        static constexpr int read_delay = (hspi_mode == dio) ? 4-1 : 0;

        void spi_init()
        {
            pinMode(sck, SPECIAL);
            pinMode(miso, SPECIAL);
            pinMode(mosi, SPECIAL);
            pinMode(cs, SPECIAL);
            spi1->spi_cmd = 0;
            GPMUX &= ~(1 << 9);
            spi1->spi_clock = spi_clkval;
            spi1->spi_ctrl = 0 ; // MSB first + plain SPI mode
            spi1->spi_ctrl1 = 0; // undocumented, clear for safety?
            spi1->spi_ctrl2 = 0; // No add'l delays on signals
            spi1->spi_user2 = 0; // No insn or insn_bits to set
        }

        // The SPI hardware cannot make the "command" portion dual or quad, only the addr and data
        // So using the command portion of the cycle will not work.  Comcatenate the address
        // and command into a single 32-bit chunk "address" which will be sent across both bits.
        void spi_writetransaction(int addr, int addr_bits, int dummy_bits, int data_bits, iotype dual)
        {
            // Ensure no writes are still ongoing
            while (spi1->spi_cmd & SPIBUSY) { /* busywait */ }

            spi1->spi_addr = addr;
            spi1->spi_user = (addr_bits? SPIUADDR : 0) | (dummy_bits ? SPIUDUMMY : 0) | (data_bits ? SPIUMOSI : 0) | (dual ? SPIUFWDIO : 0);
            spi1->spi_user1 = (addr_bits << 26) | (data_bits << 17) | dummy_bits;
            // No need to set spi_user2, insn field never used
            __asm ( "" ::: "memory" );
            spi1->spi_cmd = SPIBUSY;
            // The write may continue on in the background
        }

        uint32_t spi_readtransaction(int addr, int addr_bits, int dummy_bits, int data_bits, iotype dual)
        {
            // Ensure no writes are still ongoing
            while (spi1->spi_cmd & SPIBUSY) { /* busywait */ }

            spi1->spi_addr = addr;
            spi1->spi_user = (addr_bits? SPIUADDR : 0) | (dummy_bits ? SPIUDUMMY : 0) | SPIUMISO | (dual ? SPIUFWDIO : 0);
            spi1->spi_user1 = (addr_bits << 26) | (data_bits << 8) | dummy_bits;
            // No need to set spi_user2, insn field never used
            __asm ( "" ::: "memory" );
            spi1->spi_cmd = SPIBUSY;
            while (spi1->spi_cmd & SPIBUSY) { /* busywait */ }
            __asm ( "" ::: "memory" );
            return spi1->spi_w[0];
        }

    public:
        ESP8266SPIRAM()
        {
            /* noop */
        }
        ~ESP8266SPIRAM()
        {
            end();
        }
        void readBytes(uint32_t addr, void *destV, int count)
        {
            uint8_t *dest = (uint8_t*)destV;
            while (count > 0) {
                int toRead = std::min(count, 64);
                spi_readtransaction((0x03 << 24) | addr, 32-1, read_delay, toRead * 8 - 1, hspi_mode);
                memcpy(dest, spi1->spi_w, toRead);
                count -= toRead;
                dest += toRead;
                addr += toRead;
            }
        }

        void writeBytes(uint32_t addr, const void *srcV, int count)
        {
            const uint8_t *src = (const uint8_t*)srcV;
            while (count > 0) {
                int toWrite = std::min(count, 64);
                memcpy(spi1->spi_w, src, toWrite);
                spi_writetransaction((0x02 << 24) | addr, 32-1, 0, toWrite * 8 - 1, hspi_mode);
                count -= toWrite;
                src += toWrite;
                addr += toWrite;
             }
        }

        void begin(int freqMHz = 20)
        {
            if (freqMHz >= 40) {
                spi_clkval = 0x00001001;
            } else if (freqMHz >= 30) {
                spi_clkval = 0x00002001;
            } else if (freqMHz >= 20) {
                spi_clkval = 0x00041001;
            } else if (freqMHz >= 10) {
                spi_clkval = 0x000c1001;
            } else { // 5 MHz
                spi_clkval = 0x001c1001;
            }
            // Manually reset chip from DIO to SIO mode (HW SPI has issues with <8 bits/clocks total output)
            digitalWrite(cs, HIGH);
            digitalWrite(mosi, HIGH);
            digitalWrite(miso, HIGH);
            digitalWrite(sck, LOW);
            pinMode(cs, OUTPUT);
            pinMode(miso, OUTPUT);
            pinMode(mosi, OUTPUT);
            pinMode(sck, OUTPUT);
            digitalWrite(cs, LOW);
            for (int i = 0; i < 4; i++) {
                digitalWrite(sck, HIGH);
                digitalWrite(sck, LOW);
            }
            digitalWrite(cs, HIGH);

            // Set up the SPI regs
            spi_init();

            // Enable streaming read/write mode
            spi1->spi_w[0] = 0x40;
            spi_writetransaction(0x01<<24, 8-1, 0, 8-1, sio);

            if (hspi_mode == dio) {
                // Ramp up to DIO mode
                spi_writetransaction(0x3b<<24, 8-1, 0, 0, sio);
                spi1->spi_ctrl |= SPICDIO | SPICFASTRD;
            }
        }

        void end()
        {
            pinMode(cs, INPUT);
            pinMode(miso, INPUT);
            pinMode(mosi, INPUT);
            pinMode(sck, INPUT);
        }

};
