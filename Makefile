###################
# fko, 18/10/2007
###################
## Vous devez positionner cette variable de la sorte si vous
## installez le package du TME dans le même répertoire que
## le package de MetaScribe.
CONTAINER_TDIR			= ../executables
#CONTAINER_TDIR			= /Users/admin/MetaScribe/executables
#CONTAINER_TDIR			= /Vrac/MetaScribe/executables

EXECUTABLE_DIR			= $(CONTAINER_TDIR)/`uname -s`
SOURCES_DIR				= $(CONTAINER_TDIR)/`uname -s`_CMP

METASCRIBE				= $(EXECUTABLE_DIR)/meta_scribe
SHOW_MODEL				= $(EXECUTABLE_DIR)/show_model
XML_EXPORT				= $(EXECUTABLE_DIR)/xml_export
XML_IMPORT				= $(EXECUTABLE_DIR)/xml_import

SEARCH_DIR_OPTION       = -aL$(SOURCES_DIR) -aI$(SOURCES_DIR)

# REMARQUE: DEBUG est une variable que l'on peut definir en invoquant "make".
# C'est utile pour en savoir plus... 

###################################################
# Pour les questions...
###################################################

# Nettoyage + recompilation totale...
generator:
	make superclean
	make rules-generator
	make tengine-generator
	make test-generator

# Invocation de metascribe pour produire les sources du generateur de code
rules-generator:
	$(METASCRIBE) $(DEBUG) anlzed-pn-main.msf generator-main.mssm to-language-main.msst -v "1.0" -a "VOUS"
	gnatchop -w *.ad[sb]
	rm -f smr* syr* smt*
	mv generator_to_language.adb generator_to_language.ads

# compilation de generateur de code (en utilisant les sources de metascribe)
tengine-generator: generator_to_language.ads
	gnatmake generator_to_language.ads -gnatf $(SEARCH_DIR_OPTION) -bargs -static	 

# On lance les tests
test-generator:
	mkdir build
	./generator_to_language anlzed-pn-main.msf anlzed-pn-1.msm
	cat build/anlzed-pn.c
	mpicc runtime/runtime.c build/anlzed-pn.c -o build/anlzed-pn

# Le rapport que vous devez fournir
rapport.pdf: *.tex 
	pdflatex rapport.tex
	pdflatex rapport.tex

# A faire...
test-pour-fk:
	echo "je genere le code pour $(NAME).msm"
	echo "je compile le code genere pour le modele $(NAME)"
	echo "j'execute le code genere pour $(NAME)"

###################################################
# AUTRES...
###################################################
# On nettoie ce qui a ete construit (en particulier les sources generes par metascribe)
clean:
	rm -rf *.ali *.o *.0 *.log *.aux *.dvi *.toc *.out build

# Supprimer les fichiers generes par la compilation
bigclean:
	make clean
	rm -f *.ads *.adb 

# supprimer aussi le generateur de code produit par metascribe ainsi que les
superclean:
	make bigclean
	rm -f generator_to_language hello *.xml *.dtd rapport.pdf

# exporte le format MSM en un format XML avec sa DTD pour l'exemple donne
# (regardez l'equivalence entre MSM et XML)
rdpxml1:
	$(XML_EXPORT) -generic -f anlzed-pn-main.msf -m anlzed-pn-1.msm -o exemple
	ls *.xml *.dtd

rdpxml2:
	$(XML_EXPORT) -generic -f anlzed-pn-main.msf -m anlzed-pn-2.msm -o exemple
	ls *.xml *.dtd

# Visualiser un fichir MSM (et le tester par rapport a son MSF)
showinput1:
	$(SHOW_MODEL) anlzed-pn-main.msf anlzed-pn-1.msm
	cat ShowModel.0
	rm ShowModel.0

showinput2:
	$(SHOW_MODEL) anlzed-pn-main.msf anlzed-pn-2.msm
	cat ShowModel.0
	rm ShowModel.0

#construire une distrib pour les etudiants de master
distrib:
	mkdir ARCHIVE
	mkdir ARCHIVE/RAAR-env-projet
	cp frmwrk_msg.ENGLISH *.msm *.msf *.mssm *.msst Makefile ARCHIVE/RAAR-env-projet
	cp -R rapport.tex logo-sar.pdf examples runtime ARCHIVE/RAAR-env-projet
	cp ../READ_ME-RAAR.txt ARCHIVE
	sh -c '(cd ARCHIVE ;tar czfv ../RAAR-proj-MS.tgz RAAR-env-projet READ_ME-RAAR.txt)'
	rm -rf ARCHIVE
