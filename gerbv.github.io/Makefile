.PHONY: all
all: clean build


.PHONY: clean
clean:
	if [ -f 'index.html' ]; then	\
		rm 'index.html';	\
	fi


.PHONY: build
build:
	m4 'index.html.m4' > 'index.html'

