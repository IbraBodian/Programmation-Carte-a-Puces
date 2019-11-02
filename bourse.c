#include <io.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "tea.c"

#include <avr/eeprom.h> //pour eeprom
#include <avr/pgmspace.h> //pour ram


//------------------------------------------------
// Programme "bourse" pour carte à puce
// 
//------------------------------------------------


// déclaration des fonctions d'entrée/sortie définies dans "io.c"
void sendbytet0(uint8_t b);
uint8_t recbytet0(void);

// variables globales en static ram
uint8_t cla, ins, p1, p2, p3;	// entête de commande
uint8_t sw1, sw2;		// status word

int taille;		// taille des données introduites -- est initialisé à 0 avant la boucle
#define MAXI 20	// taille maxi des données lues
char ee_nom[MAXI] EEMEM;
uint8_t ee_taille_nom EEMEM;
uint8_t ee_solde[2] EEMEM;

#define NB_OP 4
#define MAX_BUFFER 50
typedef enum {vide=0, plein=0x1c} state_t ;
state_t ee_etat EEMEM;
typedef struct
{
  uint8_t nb_op;
  uint8_t n[NB_OP];
  uint8_t*d[NB_OP];
  uint8_t tampon[MAX_BUFFER];
}
buffer_t;
buffer_t mem_tampon EEMEM;
uint16_t compteur EEMEM;
uint32_t ee_key[4] EEMEM;

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

void engager(int n, ...)
{
  va_list args;
  uint8_t*s;
  uint8_t*d;
  uint8_t nb;
  uint8_t taille;
  
  va_start(args,n);
  eeprom_write_byte((uint8_t*)&ee_etat,vide);
  nb=0;
  taille=0;
  while (n!=0)
  {
      s=va_arg(args,uint8_t*);
      d=va_arg(args,uint8_t*);

      eeprom_write_byte(mem_tampon.n+nb,n);
      eeprom_write_block(s,mem_tampon.tampon+taille,n);
      eeprom_write_word((uint16_t*)mem_tampon.d+nb,(uint16_t)d);

      taille+=n;
      n=va_arg(args,int);
      nb++;
      
  }
  eeprom_write_byte(&mem_tampon.nb_op,nb);
  va_end(args);
  eeprom_write_byte((uint8_t*)&ee_etat,plein);
}

void valider() //eeprom -> eeprom avec intermediaire en ram
{
  uint8_t i, j;
  uint8_t nb;
  uint8_t t;
  uint8_t*d;
  uint8_t*p;
  if(eeprom_read_byte((uint8_t*)&ee_etat)==plein)
    {
      nb=eeprom_read_byte(&mem_tampon.nb_op);
      p=mem_tampon.tampon;
      for(i=0;i<nb;i++)
	{
	  t=eeprom_read_byte(mem_tampon.n+i);
	  d=(uint8_t*)eeprom_read_word((uint16_t*)mem_tampon.d+i);
	  for(j=0;j<t;j++)
	    {
	      eeprom_write_byte(d+j,eeprom_read_byte(p++));
	    }
	}
    }
  eeprom_write_byte((uint8_t*)&ee_etat,vide);
}
 
void intro_perso() //recevoir en RAM avant l'ecriture
{
    int i;
    // vérification de la taille
    if (p3>MAXI)
    {
        sw1=0x6c;	// taille incorrecte
        sw2=MAXI;	// taille attendue
        return;
    }
    sendbytet0(ins);	// acquittement
    unsigned char data[MAXI];
    for(i=0;i<p3;i++)
    {
        data[i]=recbytet0();
    }
    uint8_t solde[2];
    solde[0]=0;
    solde[1]=0;
    engager((int)p3,data,ee_nom,1,&p3,&ee_taille_nom,2,solde,ee_solde,0);
    valider();
    sw1=0x90;
}

void lecture_perso()
{
    int i;
    uint8_t taille;
    taille = eeprom_read_byte(&ee_taille_nom);
    if (p3!=taille)
    {
        sw1=0x6c;	// taille incorrecte
        sw2=taille;	// taille attendue
        return;
    }
    sendbytet0(ins);	// acquittement
    unsigned char data[p3];
    eeprom_read_block(data,ee_nom,p3);
    for(i=0;i<p3;i++)
    {
        sendbytet0(data[i]);
    }
    sw1=0x90;
}

void lecture_solde()
{
    if (p3!=2)
    {
        sw1=0x6c;	// taille incorrecte
        sw2=2;	// taille attendue
        return;
    }
    sendbytet0(ins);	// acquittement
    uint8_t solde[2];
    eeprom_read_block(solde,ee_solde,2);
    sendbytet0(solde[0]);
    sendbytet0(solde[1]);
    sw1=0x90;
}

void credit()
{
    int i;
    if (p3!=2)
    {
        sw1=0x6c;	// taille incorrecte
        sw2=2;	// taille attendue
        return;
    }
    sendbytet0(ins);	// acquittement
    uint8_t data[2];
    eeprom_read_block(data,ee_solde,2);
    uint8_t ajout[2];
    for(i=0;i<2;i++)
    {
        ajout[i]=recbytet0();
    }
    i=0;
    uint8_t somme[2];
    somme[1] = data[1] + ajout[1];
    somme[0] = data[0] + ajout[0] + (data[1] + ajout[1] < data[1] ? 1 : 0);
    if(somme[0]<data[0] || (somme[0]==data[0] && somme[1]<data[1]))
    {
	sw1=0x61;
        return;
    }
    engager(2,somme,ee_solde,0);
    valider();
    sw1=0x90;
}

void debit()
{
    int i;
    if (p3!=2)
    {
        sw1=0x6c;	// taille incorrecte
        sw2=2;	// taille attendue
        return;
    }
    sendbytet0(ins);	// acquittement
    uint8_t data[2];
    eeprom_read_block(data,ee_solde,2);
    uint8_t ajout[2];
    for(i=0;i<2;i++)
    {
        ajout[i]=recbytet0();
    }
    i=0;
    uint8_t somme[2];
    somme[1] = data[1] - ajout[1];
    somme[0] = data[0] - ajout[0] - (data[1] - ajout[1] > data[1] ? 1 : 0);
    if(somme[0]>data[0] || (somme[0]==data[0] && somme[1]>data[1]))
    {
        sw1=0x61;
        return;
    }
    engager(2,somme,ee_solde,0);
    valider();
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

	taille=0;
	sw2=0;		// pour éviter de le répéter dans toutes les commandes
  	// boucle de traitement des commandes
	valider();
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
	  	case 0x80:

		  switch(ins)
			{
			case 0:
				intro_perso();
				break;
		  	case 1:
				lecture_perso();
				break;
			case 2:
				lecture_solde();
				break;
			case 3:
			    credit();
				break;
			case 4:
			    debit();
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
