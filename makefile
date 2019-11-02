# makefile pour carte a puce
# pour compiler le projet et créer les fichiers chargeables dans la carte
# $ make
# pour effacer les fichiers créés
# $ make clean
# pour introduire le programme dans la carte
# $ make progcarte
# pour programmer les fusibles de manière à fonctionner sur l'horloge interne
# $ make fuses

# nom du fichier principal du projet
NAME 	= bourse
#nom du fichier programme
PROGNAME = $(NAME).hex

# processeur cible
PROCP 	= -mmcu=atmega328p
PROC	= m328P
AVR	= avr5

# nom du compilateur
CC = avr-gcc
# répertoire d'inclusion des entêtes de compilation
IDIR = -I/usr/lib/avr/include/avr
# répertoire de la librairie
LDIR = -L/usr/lib/avr/lib/$(AVR)/
#-DF_CPU=16000000UL

#infos sur avr-objcopy sur http://linux.die.net/man/1/avr-objcopy
# pour créer le .hex, copier ce qui est dans le .elf en format "intel-hex"
# voir https://eetools.com/downloads/about-intel-hex-file.pdf

# création des fichiers programme et eeprom par extraction du fichier elf
# avr-objcopy
# recopie une section du .elf pour réaliser un vidage hexadécimal brut
# -O = --output-target = bdfname
# -R remove any section named from the output file. May be given more than once
# -j copy only the named section

# --change-warnings
# --adjust-warnings
#    If --change-section-address or --change-section-lma or --change-section-vma
# is used, and the named section does not exist, issue a warning. This is the default. 
# --no-change-warnings
# --no-adjust-warnings
#    Do not issue a warning if --change-section-address or --adjust-section-lma or --adjust-section-vma is used, even if the named section does not exist. 


all: $(NAME).elf
	avr-objcopy -R .eeprom -R .eesafe -R .fuse -R .lock -R .signature -O ihex $(NAME).elf $(PROGNAME)


# création du fichier elf - édition de lien
# ajouter les dépendances aux autres modules
$(NAME).elf: $(NAME).o io.o
	avr-gcc -o  $(NAME).elf $(NAME).o io.o $(LDIR) $(PROCP)

# nettoyage des fichiers temporaires
clean:
	rm -f $(NAME).elf *.o $(NAME).eep $(NAME).hex

# compilation du fichier principal
$(NAME).o: $(NAME).c
	$(CC) -c  -Wall -Ofast  $(NAME).c $(PROCP) $(IDIR)

# ajouter éventuellement la compilation des autres modules
io.o: io.c
	$(CC) -c -Wall -Os io.c $(PROCP) $(IDIR)
	
# programmation de la carte à puce
progcarte:
	echo "Programmation de la carte"
	@if [ -e /dev/ttyACM0 ]; then\
		avrdude -c avrisp -p $(PROC) -P /dev/ttyACM0 -b 19200 -U flash:w:"./$(PROGNAME)":a ;else \
		avrdude -c avrisp -p $(PROC) -P /dev/ttyACM1 -b 19200 -U flash:w:"./$(PROGNAME)":a ; fi


fuses:
	echo "Programmation des fusibles"
	@if [ -e /dev/ttyACM0 ]; then\
		avrdude -c avrisp -p $(PROC) -b 19200 -P /dev/ttyACM0 -U lfuse:w:0xd2:m -U efuse:w:0xff:m -U hfuse:w:0xd9:m; else\
		avrdude -c avrisp -p $(PROC) -b 19200 -P /dev/ttyACM1 -U lfuse:w:0xd2:m -U efuse:w:0xff:m -U hfuse:w:0xd9:m; fi

