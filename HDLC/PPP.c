#include "PPP.h"

enum {
	CHECK_SUCCESS = 0,
	CHECK_ERROR_INVALID_ARGUMENT = -1,
	CHECK_ERROR_OVERRUN = -2
};

static int check_buf(ppp_buffer_t * buf)
{
	if(buf == NULL)
	{
		return CHECK_ERROR_INVALID_ARGUMENT;
	}
	if(buf->buf == NULL)
	{
		return CHECK_ERROR_INVALID_ARGUMENT;
	}
	if(buf->size == 0)
	{
		return CHECK_ERROR_INVALID_ARGUMENT;
	}
	if(buf->length > buf->size)
	{
		return CHECK_ERROR_OVERRUN;
	}
	return CHECK_SUCCESS;
}

/*
	Takes a payload and stuffs it based on PPP byte stuffing protocol. 
	Result is available in a global fixed stuffing buffer
	
	Returns the size of the stuffed buffer on success
	If there is a memory overrun, returns 0 to indicate failure
	
	Inputs:
		payload: buffer containing the data to be byte stuffed
		payload_size: the length of the data payload
		stuffed_buffer_size: the length of the working buffer. For bounds check/preventing memory overrun of the working buffer
	Outputs:
		stuffed_buffer: the working buffer containing the stuffed result
		returns: the length of the stuffed buffer, or 0 if the stuffed buffer is overrun
		
*/
size_t PPP_stuff(ppp_buffer_t * unstuffed_buffer, ppp_buffer_t * stuffed_buffer)
{
	//pre-flight check for message validity
	if(check_buf(unstuffed_buffer) != CHECK_SUCCESS)
	{
		return 0;	
	}
	if(check_buf(stuffed_buffer) != CHECK_SUCCESS)
	{
		return 0;	
	}

	size_t bidx = 0;
	for(size_t i = 0; i < unstuffed_buffer->length; i++)
	{
		uint8_t b = unstuffed_buffer->buf[i];
		if( (b == FRAME_CHAR) || (b == ESC_CHAR) )
		{
			if(bidx + 2 > stuffed_buffer->size)
			{
				return 0;
			}
			stuffed_buffer->buf[bidx++] = ESC_CHAR;
			stuffed_buffer->buf[bidx++] = b ^ ESC_MASK;
		}
		else
		{
			if(bidx >= stuffed_buffer->size)
			{
				return 0;
			}
			stuffed_buffer->buf[bidx++] = b;
		}
	}
	if(bidx >= stuffed_buffer->size)
	{
		return 0;
	}
	stuffed_buffer->buf[bidx++] = FRAME_CHAR;
	stuffed_buffer->length = bidx;
	return bidx;
}

/** @brief Helper for in-place buffer stuffing. Does in-place copy forward to prepend a byte at position 0.
 * @param byte The byte to prepend into the slice pointed to by subset
 * @returns an int 'check' code - 0 on success. Breaks public api pattern but valid for internal use. HDLC-style encoding is legacy anyway - cobs is preferred
 * */
static int prepend_byte(unsigned char byte, ppp_buffer_t * subset)
{
	int rc = check_buf(subset);
	if(rc != CHECK_SUCCESS)
	{
		return rc;
	}

	//copy forward
	if(subset->length + 1 > subset->size)
	{
		return CHECK_ERROR_OVERRUN;
	}
	for(int i = (int)(subset->length-1); i >= 0; i--)
	{
		subset->buf[i+1] = subset->buf[i];
	}
	subset->length += 1;
	subset->buf[0] = byte;
	return CHECK_SUCCESS;
}

/** @brief HDLC encode in-place 
 * O(n^2) in the worst case, since every in-place copy expansion for a byte insertion is O(n).
 * Practical reality is linear runtime - but only the PPP_stuff has O(n) guaranteed worst-case runtime.
 * 
 */
size_t PPP_stuff_single_buffer(ppp_buffer_t * msg)
{
	int rc = check_buf(msg);
	if(rc != CHECK_SUCCESS)
	{
		return 0;
	}
	size_t bidx = 0;
	while(bidx < msg->length)
	{
		uint8_t b = msg->buf[bidx];
		if( (b == FRAME_CHAR) || (b == ESC_CHAR) )
		{
			if(msg->length + 2 > msg->size)	//we need two additional bytes of overhead. preflight check
			{
				return 0;
			}
			ppp_buffer_t slice = {&msg->buf[bidx], msg->size - bidx, msg->length - bidx};
			rc = prepend_byte(ESC_CHAR, &slice);
			if(rc != CHECK_SUCCESS)
			{
				return 0;
			}
			msg->length++;
			bidx++;
			msg->buf[bidx++] = b ^ ESC_MASK;
		}
		else
		{
			if(bidx >= msg->size)
			{
				return 0;
			}
			msg->buf[bidx++] = b;
		}
	}
	if(bidx >= msg->size)
	{
		return 0;
	}
	msg->buf[bidx++] = FRAME_CHAR;
	msg->length = bidx;
	return bidx;
}

/*
	Note: This will cause a memory overrun if the size of the payload is less than half the stuffed buffer size + 2
	This is the reciprocal function of stuff_pkt
	
	Inputs: 
		stuffed_buffer: the payload, which has been stuffed
		stuffed_buffer_length: the length of the stuffed data. 
			Must be less than the actual bounds of the stuffed buffer, 
			but the function may return without scanning up to this range.
			Function will return after the first valid payload frame detected in the stuffed buffer
		payload_size: the upper bound on the size of the working buffer, payload. prevents memory overrun. Can be larger than the actual payload size, which is a returned value
		
	Outputs:
		payload: the working buffer. contains resulting unstuffed data after function returns successfully
		returns: the actual size of the payload, after unstuffing operation is complete. returns 0 on failure
*/
size_t PPP_unstuff(ppp_buffer_t * unstuffed_buffer, ppp_buffer_t * stuffed_buffer)
{
	if(check_buf(unstuffed_buffer) != CHECK_SUCCESS)
	{
		return 0;	
	}
	if(check_buf(stuffed_buffer) != CHECK_SUCCESS)
	{
		return 0;	
	}

	size_t pld_idx = 0;	//payload/working buffer index, starts at 0
	for(size_t i = 0; i < stuffed_buffer->length; i++)
	{
		 if(stuffed_buffer->buf[i] == ESC_CHAR)	//marks prepend of xored data. xor again to recover the original value
		 {
			i++; //skip to the next value
			if(i >= stuffed_buffer->length || pld_idx >= unstuffed_buffer->size)	//memory overrun guards. do two because we could overrun the while loop guards here
			{
				unstuffed_buffer->length = 0;
				return unstuffed_buffer->length;
			}
			unstuffed_buffer->buf[pld_idx++] = stuffed_buffer->buf[i] ^ ESC_MASK;
		 }
		 else if(stuffed_buffer->buf[i] == FRAME_CHAR)	//end of buffer, return 
		 {
			unstuffed_buffer->length = pld_idx;
			return unstuffed_buffer->length;	//this is the only 'valid data' case - recovered a delimited frame. Simple logic
		 }
		 else	//unaffected data, 'normal' case
		 {
			 if(pld_idx >= unstuffed_buffer->size)
			 {
				unstuffed_buffer->length = 0;
				return unstuffed_buffer->length;
			 }
			 unstuffed_buffer->buf[pld_idx++] = stuffed_buffer->buf[i];
		 }
	}
	unstuffed_buffer->length = 0;
	return unstuffed_buffer->length; //we have overrun the stuffed buffer without finding a frame character, meaning the buffer is improperly formed. return 0 length because payload is also invalid
}

/*
* For an incoming stream of data, i.e. from a serial port
* Returns payload size when the payload buffer contains a valid copy of the payload, and 0 otherwise
* 
* On an embedded system, this would be in a main loop after data is offloaded from an interrupt handler (arduino), or DMA (generic DMA on STM32 not using idle line detection for framing),
* or in an interrupt handler (generic microcontroller implementation). Creating an arduino-like queue handling between this process in main and the interrupt handler
* is preferred to avoid shared memory issues/race conditions
* 
* Wrapper for PPP_unstuff, essentially
* 
* INPUTS: 
*	new_byte:
*	payload_buffer_size:
*	input_buffer_size
* MEMORY/HELPER VARIABLES (PBR)
*	input_buffer:
*	bidx:
* OUTPUTS:
*	payload_buffer: result of PPP unstuffing
*	returns: size of the payload buffer. valid 
*/
size_t parse_PPP_stream(uint8_t new_byte, ppp_buffer_t * unstuffed_buffer, ppp_buffer_t * input)
{
	if(check_buf(unstuffed_buffer) != CHECK_SUCCESS)
	{
		return 0;
	}
	if(check_buf(input) != CHECK_SUCCESS)
	{
		return 0;
	}

	if(input->length >= input->size)
	{
		input->length = 0;
		return 0;	//overrun
	}
	input->buf[input->length++] = new_byte;
	if(new_byte == FRAME_CHAR)
	{
		size_t unstuffed_len = PPP_unstuff(unstuffed_buffer, input);
		input->length = 0;
		return unstuffed_len;	//equivalent to unstuffed_buffer.length in virtually all cases but semantically more appropriate to forward return code of unstuff, in case of a check failing.
	}
	return 0;
}
