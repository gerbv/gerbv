.PHONY: all
all: clean build


.PHONY: clean
clean:
	if [ -d 'doc' ]; then		\
		rm -rf 'doc';		\
	fi

	if [ -f 'index.html' ]; then	\
		rm 'index.html';	\
	fi


.PHONY: build
build:
	cd '..' && doxygen doc/Doxyfile.nopreprocessing
	cp -r '../doc/html' 'doc'

	bash generate_index.sh

