#include "unity.h"
#include "PPP.h"
#include "dma_style_ppp_handler.h"
#include <stdlib.h>

#define PAYLOAD_SIZE 8
#define BUFFER_SIZE 128

// Test fixtures
static uint8_t stuffing_buffer[BUFFER_SIZE];
static uint8_t unstuffing_buffer[BUFFER_SIZE];
static uint8_t example_data[PAYLOAD_SIZE];
static ppp_buffer_t stuffed_buffer;
static ppp_buffer_t input_buffer;
static ppp_buffer_t unstuffed_buffer;

void setUp(void)
{
    // Initialize buffers
    memset(stuffing_buffer, 0, BUFFER_SIZE);
    memset(unstuffing_buffer, 0, BUFFER_SIZE);
    memset(example_data, 0, PAYLOAD_SIZE);
    
    stuffed_buffer.buf = stuffing_buffer;
    stuffed_buffer.size = BUFFER_SIZE;
    stuffed_buffer.length = 0;
    
    input_buffer.buf = unstuffing_buffer;
    input_buffer.size = BUFFER_SIZE;
    input_buffer.length = 0;
    
    unstuffed_buffer.buf = example_data;
    unstuffed_buffer.size = PAYLOAD_SIZE;
    unstuffed_buffer.length = 0;
}

void tearDown(void)
{
    // Clean up if needed
}

// Helper function to print hex buffer (for debugging)
void print_hex_buffer(uint8_t * arr, int size)
{
    printf("0x");
    for(int i = 0; i < size; i++)
    {
        if(i < size-1)
            printf("%02X ", arr[i]);
        else
            printf("%02X", arr[i]);
    }
}

// Test 1: Basic PPP stuffing with random data
void test_PPP_stuff_basic_random_data(void)
{
    // Generate random test data
    for (int i = 0; i < PAYLOAD_SIZE; i++)
    {
        example_data[i] = i;
    }
    unstuffed_buffer.length = PAYLOAD_SIZE;
    
    // Perform stuffing
    int stuffed_size = PPP_stuff(&unstuffed_buffer, &stuffed_buffer);
    
    // Verify stuffing was successful
    TEST_ASSERT_GREATER_THAN(0, stuffed_size);
    TEST_ASSERT_LESS_OR_EQUAL(BUFFER_SIZE, stuffed_size);
    
    // Verify frame characters are present
	TEST_ASSERT_EQUAL(FRAME_CHAR, stuffed_buffer.buf[stuffed_size - 1]);
    
    printf("Original Data: ");
    print_hex_buffer(example_data, PAYLOAD_SIZE);
    printf("\nStuffed data: ");
    print_hex_buffer(stuffed_buffer.buf, stuffed_size);
    printf("\nSize: %d\n", stuffed_size);
}

// Test 2: PPP stuffing with frame character in data
void test_PPP_stuff_with_frame_character(void)
{
    // Create test data with frame character
    for (int i = 0; i < 3; i++)
    {
        example_data[i] = i;
    }
    example_data[3] = FRAME_CHAR;
    unstuffed_buffer.length = 4;
    
    // Perform stuffing
    int stuffed_size = PPP_stuff(&unstuffed_buffer, &stuffed_buffer);
    
    // Verify stuffing was successful
    TEST_ASSERT_GREATER_THAN(0, stuffed_size);
    TEST_ASSERT_LESS_OR_EQUAL(BUFFER_SIZE, stuffed_size);
    
    // Verify frame characters are present
    TEST_ASSERT_EQUAL(FRAME_CHAR, stuffed_buffer.buf[stuffed_size - 1]);
    
    // Verify the frame character was escaped
    TEST_ASSERT_EQUAL(ESC_CHAR, stuffed_buffer.buf[3]);
    TEST_ASSERT_EQUAL(FRAME_CHAR ^ ESC_MASK, stuffed_buffer.buf[4]);
    
    printf("Data with frame char: ");
    print_hex_buffer(example_data, 4);
    printf("\nStuffed data: ");
    print_hex_buffer(stuffed_buffer.buf, stuffed_size);
    printf("\nSize: %d\n", stuffed_size);
}

// Test 3: PPP unstuffing
void test_PPP_unstuff_basic(void)
{
    // Create test data
    for (int i = 0; i < PAYLOAD_SIZE; i++)
    {
        example_data[i] = i;
    }
    unstuffed_buffer.length = PAYLOAD_SIZE;
    
    // Stuff the data first
    int stuffed_size = PPP_stuff(&unstuffed_buffer, &stuffed_buffer);
    TEST_ASSERT_GREATER_THAN(0, stuffed_size);
    TEST_ASSERT_GREATER_THAN(0, stuffed_buffer.length);
    TEST_ASSERT_EQUAL(stuffed_buffer.length, stuffed_size);
    
    // Reset unstuffed buffer
    memset(example_data, 0, PAYLOAD_SIZE);
    unstuffed_buffer.length = 0;
    
    // Unstuff the data
    int unstuffed_size = PPP_unstuff(&unstuffed_buffer, &stuffed_buffer);
    
    // Verify unstuffing was successful
    TEST_ASSERT_EQUAL(PAYLOAD_SIZE, unstuffed_size);
    
    // Verify data matches original
    for (int i = 0; i < PAYLOAD_SIZE; i++)
    {
        TEST_ASSERT_EQUAL(example_data[i], unstuffed_buffer.buf[i]);
    }
    
    printf("Original: ");
    print_hex_buffer(example_data, PAYLOAD_SIZE);
    printf("\nUnstuffed: ");
    print_hex_buffer(unstuffed_buffer.buf, unstuffed_size);
    printf("\nSize: %d\n", unstuffed_size);
}


void test_PPP_stream_parsing_strays(void)
{
	unsigned char example_data[128] = {0};
	size_t len = 0;
	example_data[len++] = 0;
	example_data[len++] = '~';
	example_data[len++] = 1;
	example_data[len++] = 2;
	example_data[len++] = 3;
	example_data[len++] = 4;
	example_data[len++] = 5;
	example_data[len++] = '~';


	unsigned char stuff_mem[32] = {0};
	ppp_buffer_t stuffed = {stuff_mem, sizeof(stuff_mem), 0};

	unsigned char unstuff_mem[sizeof(stuff_mem)*2+2] = {0};
	ppp_buffer_t unstuffed = {unstuff_mem, sizeof(unstuff_mem), 0};

	int decode_count = 0;
	int frame_size = 0;
	for(size_t i = 0; i < sizeof(example_data); i++)
	{
		uint8_t newbyte = (uint8_t)example_data[i];
		int rc = parse_PPP_stream(newbyte, &unstuffed, &stuffed);
		if(rc != 0)
		{
			frame_size = rc;
			decode_count++;
			if(decode_count == 1)
			{
				TEST_ASSERT_EQUAL(frame_size, 1);
				TEST_ASSERT_EQUAL(frame_size, unstuffed.length);
				TEST_ASSERT_EQUAL(0, unstuffed.buf[0]);
			}
			else if (decode_count == 2)
			{
				TEST_ASSERT_EQUAL(frame_size, 5);
				TEST_ASSERT_EQUAL(frame_size, unstuffed.length);
				size_t unstuffed_idx = 0;
				for(int i = 1; i <= 5; i++)
				{
					TEST_ASSERT_EQUAL(i, unstuffed.buf[unstuffed_idx++]);
				}

			}
		}
	}
	TEST_ASSERT_EQUAL(decode_count, 2);
}




void test_PPP_stream_multiple_strays(void)
{
	unsigned char example_data[128] = {0};
	size_t len = 0;
	example_data[len++] = 0;
	example_data[len++] = '~';
	example_data[len++] = 1;
	example_data[len++] = 2;
	example_data[len++] = 3;
	example_data[len++] = 4;
	example_data[len++] = 5;
	example_data[len++] = '~';
	example_data[len++] = 0;
	example_data[len++] = '~';
	example_data[len++] = 6;
	example_data[len++] = 7;
	example_data[len++] = 8;
	example_data[len++] = '~';
	example_data[len++] = '~';	//prevent desync
	example_data[len++] = '~';
	example_data[len++] = 9;
	example_data[len++] = 10;
	example_data[len++] = '~';



	unsigned char stuff_mem[32] = {0};
	ppp_buffer_t stuffed = {stuff_mem, sizeof(stuff_mem), 0};

	unsigned char unstuff_mem[sizeof(stuff_mem)*2+2] = {0};
	ppp_buffer_t unstuffed = {unstuff_mem, sizeof(unstuff_mem), 0};

	int found_frame_count = 0;
	int frame_size = 0;
	for(size_t i = 0; i < sizeof(example_data); i++)
	{
		uint8_t newbyte = (uint8_t)example_data[i];
		int rc = parse_PPP_stream(newbyte, &unstuffed, &stuffed);
		if(rc != 0)
		{
			frame_size = rc;
			found_frame_count++;
			if(found_frame_count == 1)
			{
				TEST_ASSERT_EQUAL(frame_size, 1);
				TEST_ASSERT_EQUAL(frame_size, unstuffed.length);
				TEST_ASSERT_EQUAL(0, unstuffed.buf[0]);
			}
			else if(found_frame_count == 2)
			{
				TEST_ASSERT_EQUAL(5, unstuffed.length);
				size_t unstuffed_idx = 0;
				for(int i = 1; i <= 5; i++)
				{
					TEST_ASSERT_EQUAL(i, unstuffed.buf[unstuffed_idx++]);
				}
			}
			else if(found_frame_count == 3)
			{
				TEST_ASSERT_EQUAL(1, frame_size);
				TEST_ASSERT_EQUAL(frame_size, unstuffed.length);
				TEST_ASSERT_EQUAL(0, unstuffed.buf[0]);
			}
			else if (found_frame_count == 4)
			{
				TEST_ASSERT_EQUAL(3, unstuffed.length);
				size_t unstuffed_idx = 0;
				for(int i = 6; i <= 8; i++)
				{
					TEST_ASSERT_EQUAL(i, unstuffed.buf[unstuffed_idx++]);
				}
			}
			else if(found_frame_count == 5)
			{
				TEST_ASSERT_EQUAL(2, unstuffed.length);
				size_t unstuffed_idx = 0;
				for(int i = 9; i <= 10; i++)
				{
					TEST_ASSERT_EQUAL(i, unstuffed.buf[unstuffed_idx++]);
				}
			}
		}
	}
	TEST_ASSERT_EQUAL(5, found_frame_count);
}


void test_PPP_stream_overrun(void)
{
	
	unsigned char unstuffed_mem[8] = {};
	unsigned char streaming_mem[8*2+2] = {};
	ppp_buffer_t unstuffed_buffer = {.buf = unstuffed_mem, .length = 0, .size = sizeof(unstuffed_mem)};
	ppp_buffer_t streaming_buffer = {.buf = streaming_mem, .length = 0, .size = sizeof(streaming_mem)};
	
	int num_captured_frames = 0;
	for(size_t i = 0; i < 256; i++)
	{
		unsigned char byte = (unsigned char)i;
		if(i == 128)
		{
			byte = FRAME_CHAR;
		}
		size_t nb = parse_PPP_stream(byte, &unstuffed_buffer, &streaming_buffer);
		if(nb != 0)
		{
			num_captured_frames++;
		}
	}
	TEST_ASSERT_EQUAL(1, num_captured_frames);

}

// Test 4: PPP stream parsing
void test_PPP_stream_parsing(void)
{
    // Create test data
    for (int i = 0; i < PAYLOAD_SIZE; i++)
    {
        example_data[i] = rand() % 0x100;
    }
    unstuffed_buffer.length = PAYLOAD_SIZE;
    
    // Stuff the data
    int stuffed_size = PPP_stuff(&unstuffed_buffer, &stuffed_buffer);
    TEST_ASSERT_GREATER_THAN(0, stuffed_size);
    
    // Reset buffers for stream parsing
    memset(example_data, 0, PAYLOAD_SIZE);
    unstuffed_buffer.length = 0;
    input_buffer.length = 0;
    
    // Simulate stream parsing byte by byte
    int total_payloads_found = 0;
    for(int i = 0; i < stuffed_size; i++)
    {
        uint8_t new_byte = stuffed_buffer.buf[i];
        int pld_size = parse_PPP_stream(new_byte, &unstuffed_buffer, &input_buffer);
        
        if(pld_size != 0)
        {
            total_payloads_found++;
            printf("Stream payload found: ");
            print_hex_buffer(unstuffed_buffer.buf, pld_size);
            printf("\nSize: %d\n", pld_size);
            
            // Verify payload size matches original
            TEST_ASSERT_EQUAL(PAYLOAD_SIZE, pld_size);
        }
    }
    
    // Verify we found exactly one payload
    TEST_ASSERT_EQUAL(1, total_payloads_found);
}

// Test 5: Buffer overflow protection
void test_PPP_stuff_buffer_overflow(void)
{
    // Create a buffer that's too small
    uint8_t small_buffer[4];
    ppp_buffer_t small_stuffed_buffer = {
        .buf = small_buffer,
        .size = 4,
        .length = 0
    };
    
    // Fill with data that would cause overflow
    for (int i = 0; i < PAYLOAD_SIZE; i++)
    {
        example_data[i] = 0xAA;
    }
    unstuffed_buffer.length = PAYLOAD_SIZE;
    
    // Attempt stuffing - should fail
    int stuffed_size = PPP_stuff(&unstuffed_buffer, &small_stuffed_buffer);
    TEST_ASSERT_EQUAL(0, stuffed_size);
}

// Test 6: Invalid frame detection
void test_PPP_unstuff_invalid_frame(void)
{
    // Create invalid stuffed data (no frame character at start)
    stuffed_buffer.buf[0] = 0xAA;  // Not FRAME_CHAR
    stuffed_buffer.buf[1] = 0xBB;
    stuffed_buffer.buf[2] = 0xCC;
    stuffed_buffer.length = 3;
    
    // Attempt unstuffing - should fail
    int unstuffed_size = PPP_unstuff(&unstuffed_buffer, &stuffed_buffer);
    TEST_ASSERT_EQUAL(0, unstuffed_size);
}

// Test 7: ESC character handling
void test_PPP_stuff_with_esc_character(void)
{
    // Create test data with ESC character
    example_data[0] = ESC_CHAR;
    example_data[1] = 0xAA;
    example_data[2] = 0xBB;
    unstuffed_buffer.length = 3;
    
    // Perform stuffing
    int stuffed_size = PPP_stuff(&unstuffed_buffer, &stuffed_buffer);
    
    // Verify stuffing was successful
    TEST_ASSERT_GREATER_THAN(0, stuffed_size);
    
    // Verify the ESC character was escaped
    TEST_ASSERT_EQUAL(ESC_CHAR, stuffed_buffer.buf[0]);
    TEST_ASSERT_EQUAL(ESC_CHAR ^ ESC_MASK, stuffed_buffer.buf[1]);
    
    printf("Data with ESC char: ");
    print_hex_buffer(example_data, 3);
    printf("\nStuffed data: ");
    print_hex_buffer(stuffed_buffer.buf, stuffed_size);
    printf("\nSize: %d\n", stuffed_size);
}


void test_ppp_stream(void)
{
    
}




void test_dma_emulation(void)
{
    unsigned char rxbuf[8] = {0};
    dma_registers_t dma = {
        .CNDTR = sizeof(rxbuf),
        .CMAR = rxbuf,
        .size = sizeof(rxbuf)
    };

    unsigned char message[] = "~hello~goodbye extr characters";
    for(int i = 0; i < sizeof(message); i++)
    {
        enqueue_byte(message[i], &dma);
        if(i < sizeof(rxbuf))
        {
            TEST_ASSERT_EQUAL(message[i], rxbuf[i]);
            TEST_ASSERT_EQUAL(sizeof(rxbuf) -  dma.CNDTR, i+1); //'length' of the current message
        }
        else
        {
            TEST_ASSERT_EQUAL(dma.CNDTR, 0);
            for(int compare_idx = 0; compare_idx < sizeof(rxbuf); compare_idx++)
            {
                TEST_ASSERT_EQUAL(message[compare_idx], rxbuf[compare_idx]);
            }
        }
    }
}


void print_message(ppp_buffer_t * buf)
{
    for(int i = 0; i < buf->length; i++)
    {
        printf("%c", buf->buf[i]);
    }
    printf("\n");
}


void test_dma_handler(void)
{
    unsigned char rxbuf[18] = {0};
    dma_registers_t dma = {
        .CNDTR = sizeof(rxbuf),
        .CMAR = rxbuf,
        .size = sizeof(rxbuf)
    };


    ppp_buffer_t rx_alias = {
        .buf = rxbuf,
        .size = sizeof(rxbuf),
        .length = 0
    };
    unsigned char unstuffed[8] = {0};
    ppp_buffer_t rx_unstuffed = {
        .buf = unstuffed,
        .size = sizeof(unstuffed),
        .length = 0
    };

    unsigned char message[] = "~hello~~test~~goodbye~~~nomiss1~~nomiss2~";
    unsigned char * unstuffed_messages_chk[] = {
        "hello",
        "test",
        "goodbye",
		"nomiss1",
        "nomiss2"
    };
    int check_message_index = 0;
    for(int i = 0; i < sizeof(message); i++)
    {
        unsigned char rdr = message[i];
        enqueue_byte(rdr, &dma); //pre-interrupt handler


        //'interrupt handler'
        if(rdr == FRAME_CHAR)
        {
            // int length = sizeof(message) - dma.CNDTR;
            rx_alias.length = (rx_alias.size - (size_t)dma.CNDTR);
			dma.CNDTR = sizeof(rxbuf);
			int retlen = PPP_unstuff(&rx_unstuffed, &rx_alias);
			if(retlen != 0)
			{
				printf("DMA Message Received: ");
				print_message(&rx_unstuffed);
				for(int chk = 0; chk < rx_unstuffed.length; chk++)
				{
					TEST_ASSERT_EQUAL(unstuffed_messages_chk[check_message_index][chk], rx_unstuffed.buf[chk]);
				}
				check_message_index++;
			}
        }
    }
}




void test_stream_strings(void)
{
	const char teststr[] = "~hello~~goodbye~\0~~~~~~stray~~characters~}}}}~";
	size_t len = sizeof(teststr) - 1;	//teststr has a stray null terminator - so we have to hardcode instead of use strlen

	const char * expectedstrings[6] = {
		"hello",
		"goodbye",
		"\0",
		"stray",
		"characters",
		"]]"
	};

	unsigned char stream_mem[32] = {0};
	ppp_buffer_t stream = {
		stream_mem,
		sizeof(stream_mem),
		0
	};
	unsigned char unstuffing_mem[sizeof(stream_mem)*2+1] = {0};
	ppp_buffer_t unstuff = {
		unstuffing_mem,
		sizeof(unstuffing_mem),
		0
	};

	size_t reply_index = 0;
	for(size_t idx = 0; idx < len; idx++)
	{
		unsigned char newbyte = (unsigned char)teststr[idx];
		size_t unstuffedlen = parse_PPP_stream(newbyte, &unstuff, &stream);
		if(unstuffedlen != 0)
		{
			TEST_ASSERT_EQUAL(unstuffedlen, unstuff.length);
			TEST_ASSERT(reply_index < 6);
			size_t expected_len = strlen(expectedstrings[reply_index]);
			if(reply_index == 2)
				expected_len = 1;	//catch the /0 exception, cuz strlen won't work for that one
			TEST_ASSERT_EQUAL(expected_len, unstuff.length);
			TEST_ASSERT_EQUAL_CHAR_ARRAY(expectedstrings[reply_index], unstuff.buf, unstuff.length);			
			reply_index++;
			unstuff.buf[unstuff.length] = 0;
			printf("%s\n",unstuff.buf);
		}
	}
	TEST_ASSERT_EQUAL(6, reply_index);
	
}

void test_encode_strings_maxoverhead(void)
{
	char teststr[] = "}}}}~~~~~~~~}}}}";
	size_t len = strlen(teststr);	

	ppp_buffer_t inplace = {
		.buf = (unsigned char *)teststr,
		.size = sizeof(teststr),
		.length = len
	};
	TEST_ASSERT_EQUAL(0, PPP_stuff_single_buffer(&inplace));	//this should fail due to lack of overhead

	ppp_buffer_t unstuffed = {
		(unsigned char *)teststr,
		sizeof(teststr),
		len
	};
	unsigned char stuff_mem_small_1[sizeof(teststr) - 1] = {0};
	unsigned char stuff_mem_small_2[(sizeof(teststr) - 1)*2] = {0};	//knock off the null terminator
	unsigned char stuff_mem_just_right[(sizeof(teststr)-1)*2+1] = {0}; //includes null terminator
	//OVERHEAD IS INPUT*2 + 1
	
	ppp_buffer_t stuffed = {
		(unsigned char *)stuff_mem_small_1,
		sizeof(stuff_mem_small_1),
		.length = 0
	};
	TEST_ASSERT_EQUAL(0, PPP_stuff(&unstuffed, &stuffed));

	stuffed = (ppp_buffer_t){
		(unsigned char *)stuff_mem_small_2,
		sizeof(stuff_mem_small_2),
		.length = 0
	};
	TEST_ASSERT_EQUAL(0, PPP_stuff(&unstuffed, &stuffed));

	stuffed = (ppp_buffer_t){
		(unsigned char *)stuff_mem_just_right,
		sizeof(stuff_mem_just_right),
		.length = 0
	};
	TEST_ASSERT_EQUAL(len*2+1, PPP_stuff(&unstuffed, &stuffed));

	memset(unstuffed.buf, 0, unstuffed.size);
	TEST_ASSERT_EQUAL(len, PPP_unstuff(&unstuffed, &stuffed));
	TEST_ASSERT_EQUAL(len, unstuffed.length);
	TEST_ASSERT_EQUAL_CHAR_ARRAY(teststr, unstuffed.buf, unstuffed.length);
}


void test_ppp_encode_in_place_maxoverhead(void)
{
	char teststr[] = "}}}}~~~~~~~~}}}}";
	size_t len = strlen(teststr);	
	ppp_buffer_t inplace;

	unsigned char onebyte_undersized[(sizeof(teststr)-1)*2];
	unsigned char correct_size[(sizeof(teststr)-1)*2+1];

	unsigned char unstuff_mem[sizeof(teststr) - 1] = {0};
	ppp_buffer_t unstuff = {
		unstuff_mem,
		sizeof(unstuff_mem),
		0
	};
	
	inplace = (ppp_buffer_t){		//obviously undersized, in-place
		.buf = (unsigned char *)teststr,
		.size = sizeof(teststr),
		.length = len
	};
	TEST_ASSERT_EQUAL(2, PPP_unstuff(&unstuff, &inplace));	//technically teststr is a valid stuffed frame up to the first delimiter	
	TEST_ASSERT_EQUAL(0, PPP_stuff_single_buffer(&inplace));	//this should fail due to lack of overhead

	strcpy(onebyte_undersized,  teststr);
	inplace = (ppp_buffer_t){
		.buf = onebyte_undersized,
		.size = sizeof(onebyte_undersized),
		.length = len
	};
	TEST_ASSERT_EQUAL(0, PPP_stuff_single_buffer(&inplace));

	strcpy(correct_size,  teststr);
	inplace = (ppp_buffer_t){
		.buf = correct_size,
		.size = sizeof(correct_size),
		.length = len
	};
	TEST_ASSERT_EQUAL(len*2+1, PPP_stuff_single_buffer(&inplace));
	TEST_ASSERT_EQUAL(len*2+1, inplace.length);
	
	memset(unstuff.buf, 0, unstuff.size);
	size_t unstuff_size = PPP_unstuff(&unstuff, &inplace);
	TEST_ASSERT_EQUAL_CHAR_ARRAY(teststr, unstuff.buf, len);
}


void test_ppp_encode_in_place_mixed(void)
{
	char teststr[] = "}}asdfasdfa}}~~~asf~6734734~2~~~}sadfas}}}";
	size_t len = strlen(teststr);	

	unsigned char correct_size[(sizeof(teststr)-1)*2+1];
	strcpy(correct_size,  teststr);
	ppp_buffer_t inplace = (ppp_buffer_t){
		.buf = correct_size,
		.size = sizeof(correct_size),
		.length = len
	};

	size_t stuffed_len = PPP_stuff_single_buffer(&inplace);
	TEST_ASSERT_NOT_EQUAL(0, stuffed_len);
	TEST_ASSERT_EQUAL(stuffed_len, inplace.length);
	
	unsigned char unstuff_mem[sizeof(teststr) - 1] = {0};
	ppp_buffer_t unstuff = {
		unstuff_mem,
		sizeof(unstuff_mem),
		0
	};
	memset(unstuff.buf, 0, unstuff.size);
	size_t unstuff_size = PPP_unstuff(&unstuff, &inplace);
	TEST_ASSERT_EQUAL_CHAR_ARRAY(teststr, unstuff.buf, len);
}



void test_ppp_encode_copy_mixed(void)
{
	char teststr[] = "}}asdfasdfa}}~~~asf~6734734~2~~~}sadfas}}}";
	size_t len = strlen(teststr);	
	ppp_buffer_t inplace = (ppp_buffer_t){
		.buf = teststr,
		.size = len,
		.length = len
	};
	unsigned char correct_size[(sizeof(teststr)-1)*2+1];
	ppp_buffer_t stuffed = {
		correct_size,
		sizeof(correct_size),
		0	
	};
	size_t stuffed_len = PPP_stuff(&inplace, &stuffed);
	TEST_ASSERT_NOT_EQUAL(0, stuffed_len);
	TEST_ASSERT_EQUAL(stuffed_len, stuffed.length);
	TEST_ASSERT_EQUAL(len, inplace.length);	//sanity check

	unsigned char unstuff_mem[sizeof(teststr) - 1] = {0};
	ppp_buffer_t unstuff = {
		unstuff_mem,
		sizeof(unstuff_mem),
		0
	};
	memset(unstuff.buf, 0, unstuff.size);
	size_t unstuff_size = PPP_unstuff(&unstuff, &stuffed);
	TEST_ASSERT_EQUAL(len, unstuff_size);
	TEST_ASSERT_EQUAL(unstuff_size, unstuff.length);
	TEST_ASSERT_EQUAL_CHAR_ARRAY(teststr, unstuff.buf, len);
}
