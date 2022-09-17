#include <avr/io.h>
#include <avr/interrupt.h>
#include "TWI_lib.h"
#include "util/delay.h"
#include <stdio.h>
#include "../avr_common/uart.h"

void TWI_Init(){
	TWI_info.mode = Ready;
	TWI_info.error_code = TWI_SUCCESS;
	TWI_info.repeated_start = 0;
	TWSR = 0;                                 //no prescaling 
	TWBR = ((F_CPU / TWI_FREQ) - 16) / 2;     //setta il bit rate
	TWCR = (1 << TWIE) | (1 << TWEN);         //abilita TWI
	TB_Index=0;                               //valore iniziale TB_Index (per sicurezza, consizìderare di togliere)
	RB_Index=0; 							  //valore iniziale RB_Index  (per sicurezza, considerare di toglierla)
}

uint8_t is_TWI_ready(){
	if (TWI_info.mode == Ready) return 1;
	else return 0;
}

uint8_t TWI_Transmit_Data(void *const TR_data, uint8_t data_len, uint8_t repeated_start){
	printf_init();
	if (data_len <= TRANSMIT_BUFLEN){
		while (!is_TWI_ready() ) { _delay_us(1);}
		TWI_info.mode = Initializing;             
		TWI_info.repeated_start = repeated_start;
		uint8_t *data = (uint8_t *)TR_data;
		for (int i = 0; i < data_len; i++){
			Transmit_Buffer[i] = data[i];
		}
		transmit_len = data_len;
		TB_Index = 0;
		if (TWI_info.mode == Repeated_Start){
			TWDR = Transmit_Buffer[TB_Index++]; 
			TWI_Send_Transmit(); 
		}
		else{ TWI_Send_Start();}
	}
	else{ return 1; }
	return 0;
}

uint8_t TWI_Read_Data(uint8_t TWI_addr, uint8_t bytes_to_read, uint8_t repeated_start){
	if (bytes_to_read < RECEIVE_BUFLEN){
		RB_Index = 0;
		receive_len = bytes_to_read;
		uint8_t TR_data[1];
		TR_data[0] = (TWI_addr << 1) | 0x01;
		TWI_Transmit_Data(TR_data, 1, repeated_start);
		while (!is_TWI_ready()){_delay_us(1);}
	}
	else return 1;
	return 0;
}

uint8_t TWI_Slave_Transmit_Data(uint8_t SL_addr, void *const TR_data, uint8_t data_len){
	if (data_len <= TRANSMIT_BUFLEN){
		TWI_info.mode = Initializing;
		TB_Index=0;
		transmit_len=data_len;
		TWAR= (SL_addr << 1) | 0x01;          //LSB=1 per il riconoscimento del broadcast
		TWI_Set_Address();
		uint8_t *data = (uint8_t *)TR_data;
		for (int i =0; i< data_len; i++){
			Transmit_Buffer[i]= data[i];
		}
		while (!is_TWI_ready()) { _delay_us(1);}
	}
	else return 1; 
	return 0; 
}

uint8_t TWI_Slave_Receive_Data(uint8_t SL_addr){    
	TWI_info.mode = Initializing;
	TWAR= (SL_addr << 1) | 0x01;          //LSB=1 per il riconoscimento del broadcast
	TWI_Set_Address();
	while (!is_TWI_ready()) { _delay_us(1);}
	return 0; 
}

ISR (TWI_vect){
	
	switch (TWI_STATUS){
		
		case REP_START_TRANSMITTED:
			printf("REP_START_TRANSMITTED\n");
			TWI_info.mode=Repeated_Start;
		
		case START_TRANSMITTED:
			printf("START_TRANSMITTED\n");
			TWDR=Transmit_Buffer[TB_Index++]; 
			TWI_Send_Transmit();
			break;
			
		case SLAW_TR_ACK_RV:
			printf("SLAW_TR_ACK_RV\n");
			TWI_info.mode = Master_Transmitter;
			
		case M_DATA_TR_ACK_RV:
			printf("M_DATA_TR_ACK_RV\n");
		
			if (TB_Index < transmit_len){
				TWDR=Transmit_Buffer[TB_Index++];
				TWI_Send_Transmit();
			}
			else if (TWI_info.repeated_start){
				TWI_Send_Start();
			}
			else{
				TWI_info.mode=Ready;
				TWI_info.error_code=TWI_SUCCESS;
				TWI_Send_Stop();
			}	
			break;
			
		case SLAW_TR_NACK_RV:
			printf("SLAW_TR_NACK_RV\n");
		
		case M_DATA_TR_NACK_RV:
			printf("M_DATA_TR_NACK_RV\n");
			
		case SLAR_TR_NACK_RV:
			printf("SLAR_TR_NACK_RV\n");
		
		case ARBITRATION_LOST:
			printf("ARBITRATION_LOST\n");
			
			if (TWI_info.mode == Initializing){
				TB_Index--; 
				TWI_Send_Start();
				break;
			}
		
			TWI_info.error_code = TWI_STATUS;
		
			if (TWI_info.repeated_start){
				TWI_Send_Start();
			}
			else{
				TWI_info.mode=Ready;
				TWI_Send_Stop();
			}	
			break;
		
		case SLAR_TR_ACK_RV:
			printf("SLAR_TR_ACK_RV\n");
			TWI_info.mode=Master_Receiver;
			if (RB_Index < receive_len -1){
				TWI_info.error_code = TWI_NO_RELEVANT_INFO;
				TWI_Send_ACK();
			}
			else{
				TWI_info.error_code = TWI_NO_RELEVANT_INFO;
				TWI_Send_NACK();
			}
			break;
		
		case DATA_RV_ACK_TR:
			printf("DATA_RV_ACK_TR\n");
			Receive_Buffer[RB_Index++]=TWDR;
			if (RB_Index < receive_len -1){
				TWI_info.error_code = TWI_NO_RELEVANT_INFO;
				TWI_Send_ACK();
			}
			else{
				TWI_info.error_code = TWI_NO_RELEVANT_INFO;
				TWI_Send_NACK();
			}
			break;
		
		case DATA_RV_NACK_TR:
			printf("DATA_RV_NACK_TR\n");
			Receive_Buffer[RB_Index++] = TWDR;
			TWI_info.error_code = TWI_SUCCESS;	
			if (TWI_info.repeated_start){
				TWI_Send_Start();
			}
			else{
				TWI_info.mode = Ready;
				TWI_Send_Stop();
			}
			break;
			
		case ARBITRATION_LOST_SR_ADDR:
			printf("ARBITRATION_LOST_SR_ADDR\n");
		
		case ARBITRATION_LOST_SR_BRD:
			printf("ARBITRATION_LOST_SR_BRD\n");
		
		case BRDW_RV_ACK_TR:
			printf("BRDW_RV_ACK_TR\n");
			
		case SLAW_RV_ACK_TR:
			printf("SLAW_RV_ACK_TR\n");
			TWI_info.mode=Slave_Receiver;
			if (RB_Index < RECEIVE_BUFLEN -1){                     
				TWI_info.error_code = TWI_NO_RELEVANT_INFO;
				TWI_Send_ACK();
			}
			else{
				TWI_info.error_code = TWI_NO_RELEVANT_INFO;
				TWI_Send_NACK();
			}
			break;
			
		case DATA_BRD_RV_ACK_TR:
			printf("DATA_BRD_RV_ACK_TR\n");
		
		case DATA_SLA_RV_ACK_TR:
			printf("DATA_SLA_RV_ACK_TR\n");
			Receive_Buffer[RB_Index++] = TWDR; 
			if (RB_Index < RECEIVE_BUFLEN-1){                
				TWI_info.error_code = TWI_NO_RELEVANT_INFO;
				TWI_Send_ACK();
			}
			else{
				TWI_info.error_code = TWI_NO_RELEVANT_INFO;
				TWI_Send_NACK();
			}
			break;
			
		case DATA_BRD_RV_NACK_TR:      //forse qui vanno inviati degli ACK/NACK?
			printf("DATA_BRD_RV_NACK_TR\n");
		
		case DATA_SLA_RV_NACK_TR:
			printf("DATA_SLA_RV_NACK_TR\n");
			Receive_Buffer[RB_Index++] = TWDR;
			TWI_info.error_code = TWI_SUCCESS;	
			if (TWI_info.repeated_start){}
			else{
				TWI_info.mode = Ready;
			}
			break;
			
		case S_DATA_TR_NACK_RV:
			printf("S_DATA_TR_NACK_RV\n");
		
		case S_LDATA_TR_ACK_RV:
			printf("S_LDATA_TR_ACK_RV\n");
		
		case START_STOP_FOR_SLAVE:     
			printf("START_STOP_FOR_SLAVE\n");
			TWI_info.error_code = TWI_STATUS;
			TWI_Send_NACK();	
			if (TWI_info.repeated_start){}
			else{
				TWI_info.mode = Ready;
			}
			break;
		
		case ARBITRATION_LOST_ST:
			printf("ARBITRATION_LOST_ST\n");
		
		case SLAR_RV_ACK_TR:
			printf("SLAR_RV_ACK_TR\n");
			TWI_info.mode=Slave_Transmitter;
			TWI_Set_Address();
		
		case S_DATA_TR_ACK_RV:
			printf("S_DATA_TR_ACK_RV\n");
			TWDR= Transmit_Buffer[TB_Index++]; 
			if (TB_Index < transmit_len){                    
				TWI_info.error_code = TWI_NO_RELEVANT_INFO;
				TWI_Send_ACK();
			}
			else{
				TWI_info.error_code = TWI_NO_RELEVANT_INFO;
				TWI_Send_NACK();
			}
			break;
		
		case TWI_NO_RELEVANT_INFO:
			printf("TWI_NO_RELEVANT_INFO\n");
			break;
		
		case TWI_ILLEGAL_START_STOP:
			printf("TWI_ILLEGAL_START_STOP\n");
			TWI_info.mode=Ready;
			TWI_Send_Stop();
			break;
	}
}

