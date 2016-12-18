all: index.html emiplib.html

emiplib.html: emiplib.md
	./md.py emiplib.md > emiplib.html

index.html: emiplib.html
	cp emiplib.html index.html

clean:
	rm -f emiplib.html index.html
