#pragma once

#include <stdbool.h>
#include <stdint.h>

/******************************************************************************
 * CLASS
 *****************************************************************************/
/* Events emited by generic PSU state machine */
enum generic_psu_event
{
	GENERIC_PSU_EVENT_NONE,
	GENERIC_PSU_EVENT_STARTED,

	/* All related resources can be safely deallocated at this point */
	GENERIC_PSU_EVENT_STOPPED
};

/* Events emited by hardware and passed to generic PSU state machine */
enum generic_psu_hw_event
{
	GENERIC_PSU_HW_EVENT_NONE,
	GENERIC_PSU_HW_EVENT_READY,
	GENERIC_PSU_HW_EVENT_FAULT
};

/* PSU flags */
enum generic_psu_flags
{
	GENERIC_PSU_FLAG_ACK_SETTINGS = 1,
	GENERIC_PSU_FLAG_HW_OK        = 2
};

enum generic_psu_state
{
	GENERIC_PSU_STATE_INIT,
	GENERIC_PSU_STATE_WAIT_FOR_INITIAL_SETTINGS,
	GENERIC_PSU_STATE_WAIT_INITIAL_HARDWARE_RESPONSE,
	GENERIC_PSU_STATE_RUNNING,
	GENERIC_PSU_STATE_PRE_ERROR,
	GENERIC_PSU_STATE_ERROR
};

/* Initial settings */
struct generic_psu_settings
{
	uint16_t max_in_voltage_V;
	uint16_t min_in_voltage_V;

	int16_t max_in_current_A;
	int16_t min_in_current_A;

	uint16_t max_out_voltage_V;
	uint16_t min_out_voltage_V;

	int16_t max_out_current_A;
	int16_t min_out_current_A;

	uint32_t timeout_ms;
};

struct generic_psu {
	uint8_t _state;

	/* Settings */
	struct generic_psu_settings _settings;

	/* Runtime */
	uint16_t _set_out_voltage_V;
	int16_t  _set_out_current_A;

	uint16_t _out_voltage_V;
	int16_t  _out_current_A;

	uint8_t _flags;

	uint32_t _timer_ms;
	uint32_t _timeout_ms;
	uint32_t _overcurrent_timer;
	uint32_t _overvoltage_timer;
};

/******************************************************************************
 * PRIVATE
 *****************************************************************************/
void _generic_psu_check_working_conditions(struct generic_psu *self,
					   uint32_t delta_time_ms)
{
	if ((self->_out_current_A > self->_settings.max_out_current_A) ||
	    (self->_out_current_A < self->_settings.min_out_current_A)) {
		self->_overcurrent_timer += delta_time_ms;
	} else {
		self->_overcurrent_timer = 0U;
	}

	if ((self->_out_voltage_V > self->_settings.max_out_voltage_V) ||
	    (self->_out_voltage_V < self->_settings.min_out_voltage_V)) {
		self->_overvoltage_timer += delta_time_ms;
	} else {
		self->_overvoltage_timer = 0U;
	}

	if ((self->_overcurrent_timer > 1000U) ||
	    (self->_overvoltage_timer > 1000U)) {
    		self->_state = GENERIC_PSU_STATE_PRE_ERROR;
	}
}

void _generic_psu_check_state_timeout(struct generic_psu *self,
				      uint32_t delta_time_ms)
{
	self->_timer_ms += delta_time_ms;

	if (self->_timer_ms >= self->_timeout_ms) {
		self->_state = GENERIC_PSU_STATE_PRE_ERROR;
	}
}

/******************************************************************************
 * PUBLIC
 *****************************************************************************/
/* Init with default values */
void generic_psu_init(struct generic_psu *self)
{
	self->_state = GENERIC_PSU_STATE_INIT;

	/* Settings */
	self->_settings.max_in_voltage_V = 0U;
	self->_settings.min_in_voltage_V = 0U;

	self->_settings.max_in_current_A = 0;
	self->_settings.min_in_current_A = 0;

	self->_settings.max_out_voltage_V = 0U;
	self->_settings.min_out_voltage_V = 0U;

	self->_settings.max_out_current_A = 0;
	self->_settings.min_out_current_A = 0;

	self->_settings.timeout_ms = 15000U; /* 15 seconds state timeout */

	/* Runtime */
	self->_set_out_voltage_V = 0U;
	self->_set_out_current_A = 0;

	self->_out_voltage_V = 0U;
	self->_out_current_A = 0;
	
	self->_flags = 0;

	self->_timer_ms          = 0;
	self->_timeout_ms        = 0;
	self->_overcurrent_timer = 0;
	self->_overvoltage_timer = 0;
}

/* Setters */

/* Ack setting has been configured */
void generic_psu_set_ack_settings(struct generic_psu *self)
{
	self->_flags |= GENERIC_PSU_FLAG_ACK_SETTINGS;
}

void generic_psu_set_out_voltage_V(struct generic_psu *self, uint16_t voltage)
{
	const uint16_t max_val = self->_settings.max_out_voltage_V;
	const uint16_t min_val = self->_settings.min_out_voltage_V;
	uint16_t new_val = voltage;

	/* Clamp value */
	new_val = (new_val > max_val) ? max_val : new_val;
	new_val = (new_val < min_val) ? min_val : new_val;
	
	self->_set_out_voltage_V = new_val;
}

void generic_psu_set_out_current_A(struct generic_psu *self, int16_t current)
{
	const int16_t max_val = self->_settings.max_out_current_A;
	const int16_t min_val = self->_settings.min_out_current_A;
	int16_t new_val = current;

	/* Clamp value */
	new_val = (new_val > max_val) ? max_val : new_val;
	new_val = (new_val < min_val) ? min_val : new_val;
	
	self->_set_out_current_A = new_val;
}

void generic_psu_set_timeout_ms(struct generic_psu *self,
				    uint32_t timeout_ms)
{
	self->_timeout_ms = timeout_ms;
}

void generic_psu_set_hw_event(struct generic_psu *self, uint8_t event)
{
	switch (event) {
	case GENERIC_PSU_HW_EVENT_READY:
		self->_flags |= GENERIC_PSU_FLAG_HW_OK;
		break;
		
	case GENERIC_PSU_HW_EVENT_FAULT:
		self->_flags &= ~GENERIC_PSU_FLAG_HW_OK;
		break;

	default:
		break;
	}
}

void generic_psu_set_hw_out_voltage_V(struct generic_psu *self,
				      uint16_t voltage)
{
	self->_out_voltage_V = voltage;
}

void generic_psu_set_hw_out_current_A(struct generic_psu *self,
				      int16_t current)
{
	self->_out_current_A = current;
}

/* Getters */

uint16_t generic_psu_get_out_voltage_V(struct generic_psu *self)
{
	return self->_out_voltage_V;
}

int16_t generic_psu_get_out_current_A(struct generic_psu *self)
{
	return self->_out_current_A;
}


/* Update */

enum generic_psu_event generic_psu_update(struct generic_psu *self,
					  uint32_t delta_time_ms)
{
	enum generic_psu_event e = GENERIC_PSU_EVENT_NONE;

	switch (self->_state) {
	case GENERIC_PSU_STATE_INIT:
		e = GENERIC_PSU_EVENT_STARTED;
		self->_timer_ms = 0;
		self->_state =
			GENERIC_PSU_STATE_WAIT_FOR_INITIAL_SETTINGS;
		break;
	
	case GENERIC_PSU_STATE_WAIT_FOR_INITIAL_SETTINGS:
		if ((self->_flags & 
		     (uint8_t)GENERIC_PSU_FLAG_ACK_SETTINGS) > 0U) {
			self->_timer_ms   = 0;
			self->_timeout_ms = self->_settings.timeout_ms;
			self->_state =
			      GENERIC_PSU_STATE_WAIT_INITIAL_HARDWARE_RESPONSE;
		}

		_generic_psu_check_state_timeout(self, delta_time_ms);

		break;

	case GENERIC_PSU_STATE_WAIT_INITIAL_HARDWARE_RESPONSE:

		if ((self->_flags & (uint8_t)GENERIC_PSU_FLAG_HW_OK) > 0U) {
			self->_state = GENERIC_PSU_STATE_RUNNING;
		}

		_generic_psu_check_state_timeout(self, delta_time_ms);
		
		break;

	case GENERIC_PSU_STATE_RUNNING:
		if (!(self->_flags & (uint8_t)GENERIC_PSU_FLAG_HW_OK)) {
			self->_state = GENERIC_PSU_STATE_PRE_ERROR;
		}

		_generic_psu_check_working_conditions(self, delta_time_ms);

		break;

	case GENERIC_PSU_STATE_PRE_ERROR:
		self->_state = GENERIC_PSU_STATE_ERROR;
		e = GENERIC_PSU_EVENT_STOPPED;
		break;

	case GENERIC_PSU_STATE_ERROR:
		break;

	default:
		break;
	}

	return e;
}
