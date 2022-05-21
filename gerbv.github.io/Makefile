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
	make -C '..' doxygen
	cp -r '../doc/html' 'doc'

	m4 'index.html.m4' > 'index.html'

