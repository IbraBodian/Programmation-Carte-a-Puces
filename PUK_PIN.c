#include <io.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "tea.c"

#include <avr/eeprom.h> //pour eeprom
#include <avr/pgmspace.h> //pour ram


//------------------------------------------------
// Programme "code" pour carte à puce
// 
//------------------------------------------------


// déclaration des fonctions d'entrée/sortie définies dans "io.c"
void sendbytet0(uint8_t b);
uint8_t recbytet0(void);

// variables globales en static ram
uint8_t cla, ins, p1, p2, p3;	// entête de commande
uint8_t sw1, sw2;		// status word


typedef enum {vierge=0, verrouille=1, deverouille=2, bloque=3} state_t ;
state_t etat;
uint8_t ee_code_pin[8] EEMEM;
uint32_t ee_puk[2] EEMEM;
uint8_t ee_nb_essais_restant EEMEM = 0xff;

// Procédure qui renvoie l'ATR
void atr(uint8_t n, char* hist)
{
    	sendbytet0(0x3b);	// définition du protocole
    	sendbytet0(n);		// nombre d'octets d'historique
    	while(n--)		// Boucle d'envoi des octets d'historique
    	{
        	sendbytet0(*hist++);
    	}
}

state_t get_state()
{
  switch(eeprom_read_byte(&ee_nb_essais_restant))
    {
    case 0:
      return bloque;
    case 0xff:
      return vierge;
    default:
      return verrouille;
    }

}


int compare_fst(uint8_t test[8], uint8_t code[8])
{
  int i;
  for(i=0;i<8;i++)
    {
      if(test[i]!=code[i]) return 1;
    }
  return 0;
}

int compare_cst(uint8_t test[8], uint8_t code[8]) //test en temps constant pour contrer attaque temporelle
{
  int i;
  uint8_t code_bon=0;
  for(i=0;i<8;i++)
    {
      code_bon|=test[i]^code[i];
    }
  return code_bon;
}

static int(*compare[2])(uint8_t*, uint8_t*)={compare_fst,compare_cst}; //p1 vaut 0 => mauvaise fct compare ; p1=1 => bonne fct




void intro_puk()
{
    int i;
    uint8_t data[8];
    if(etat!=vierge)
    {
		sw1=0x6d;
        return;
    }
    
    if (p3!=8)
    {
        sw1=0x6c;	// taille incorrecte
        sw2=8;	// taille attendue
        return;
    }
    sendbytet0(ins);	// acquittement
    
    for(i=0;i<8;i++) data[i]=recbytet0();
    
    
	uint32_t tea_puk[2]={1,2};
	uint32_t crypto[2];
	uint32_t k[4]={65536*data[0]+data[1],65536*data[2]+data[3],65536*data[4]+data[5],65536*data[6]+data[7]};
	tea_chiffre(tea_puk, crypto, k);
	eeprom_write_block(crypto,ee_puk,8);
	
   
    for(i=0;i<8;i++) eeprom_write_byte(ee_code_pin+i,'0');
    eeprom_write_byte(&ee_nb_essais_restant,3);
    etat=verrouille;
    sw1=0x90;
}

void change_pin()
{
  if(etat!=deverouille)
  {
		sw1=0x6d;
        return;
  }
  int i, nbe;
  uint8_t data2[8];
  uint8_t pin[8];
    if (p3!=16)
    {
        sw1=0x6c;	// taille incorrecte
        sw2=16;	// taille attendue
        return;
    }
    sendbytet0(ins);	// acquittement
    
    for(i=0;i<8;i++) pin[i]=recbytet0();
    for(i=0;i<8;i++) data2[i]=recbytet0();
    
    uint8_t data[8];
    eeprom_read_block(data,ee_code_pin,8);
    
    if(compare[p1&1](pin,data)!=0)
	{
	  nbe=eeprom_read_byte(&ee_nb_essais_restant)-1;
	  eeprom_write_byte(&ee_nb_essais_restant,nbe);
	  if(nbe==0) etat=bloque;
	  sw1=0x98;
	  sw2=0x40+nbe;
	  return;
    }
    eeprom_write_byte(&ee_nb_essais_restant,3);
    
    eeprom_write_block(data2,ee_code_pin,8);
    sw1=0x90;
}

void verify_pin()
{
  int i, nbe;
  uint8_t pin[8];
    if(etat!=verrouille)
    {
	sw1=0x6d;
        return;
    }
    if (p3!=8)
    {
        sw1=0x6c;	// taille incorrecte
        sw2=8;	// taille attendue
        return;
    }
    sendbytet0(ins);	// acquittement
    for(i=0;i<8;i++) pin[i]=recbytet0();
    uint8_t data[8];
    eeprom_read_block(data,ee_code_pin,8);
    
    if(compare[p1&1](pin,data)!=0)
      {
	  nbe=eeprom_read_byte(&ee_nb_essais_restant)-1;
	  eeprom_write_byte(&ee_nb_essais_restant,nbe);
	  if(nbe==0) etat=bloque;
	  sw1=0x98;
	  sw2=0x40+nbe;
	  return;
	}
    etat=deverouille;
    sw1=0x90;
}

void deblocage()
{
    int i;
    uint8_t puk[8], pin[8];
    if(etat!=bloque)
    {
	sw1=0x6d;
        return;
    }
    if (p3!=16)
    {
        sw1=0x6c;	// taille incorrecte
        sw2=16;	// taille attendue
        return;
    }
    sendbytet0(ins);	// acquittement
    for(i=0;i<8;i++) puk[i]=recbytet0();
    for(i=0;i<8;i++) pin[i]=recbytet0();
 
    uint32_t data[2];
    uint32_t tea_puk[2]={1,2};
	uint32_t crypto[2];
    
    eeprom_read_block(data,ee_puk,8);
    uint32_t k[4]={65536*puk[0]+puk[1],65536*puk[2]+puk[3],65536*puk[4]+puk[5],65536*puk[6]+puk[7]};
	tea_chiffre(tea_puk, crypto, k);
    uint8_t cryptoN[8]={crypto[0]>>24,(crypto[0]&0b111111110000000000000000)>>16,(crypto[0]&0b1111111100000000)>>8,crypto[0]&0b11111111,crypto[1]>>24,(crypto[1]&0b111111110000000000000000)>>16,(crypto[1]&0b1111111100000000)>>8,crypto[1]&0b11111111};
    uint8_t dataN[8]={data[0]>>24,(data[0]&0b111111110000000000000000)>>16,(data[0]&0b1111111100000000)>>8,data[0]&0b11111111,data[1]>>24,(data[1]&0b111111110000000000000000)>>16,(data[1]&0b1111111100000000)>>8,data[1]&0b11111111};
    if(compare[p1&1](cryptoN,dataN)!=0){sw1=0x98; sw2=4; return;};   
    eeprom_write_block(pin,ee_code_pin,8);
    etat=verrouille;
    eeprom_write_byte(&ee_nb_essais_restant,3);
    sw1=0x90;
}

// Programme principal
//--------------------
int main(void)
{
  	// initialisation des ports
	ACSR=0x80;
	DDRB=0xff;
	DDRC=0xff;
	DDRD=0;
	PORTB=0xff;
	PORTC=0xff;
	PORTD=0xff;
	ASSR=1<<EXCLK;
	TCCR2A=0;
	ASSR|=1<<AS2;


	// ATR
  	atr(11,"Hello scard");

	etat=get_state();
	sw2=0;		// pour éviter de le répéter dans toutes les commandes
  	// boucle de traitement des commandes
  	for(;;)
  	{
    	// lecture de l'entête
    	cla=recbytet0();
    	ins=recbytet0();
    	p1=recbytet0();
	p2=recbytet0();
    	p3=recbytet0();
	    sw2=0;
		switch (cla)
		{
	  	case 0xa0:

		  switch(ins)
			{
			case 0x20:
				verify_pin();
				break;
		  	case 0x24:
				change_pin();
				break;
			case 0x2c:
				deblocage();
				break;
			case 0x40:
			        intro_puk();	    
				break;
			default:
			        sw1=0x6d; // code erreur ins inconnu
			}
		        break;
		default:
		        sw1=0x6e; // code erreur classe inconnue
		}
		sendbytet0(sw1); // envoi du status word
		sendbytet0(sw2);
  	}
  	return 0;
}
