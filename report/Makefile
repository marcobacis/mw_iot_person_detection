NAME = report

ifeq ($(shell uname -s),Darwin)
PDF_READER=open
else
PDF_READER=evince
endif

paper:
	@rm -f *.aux *.bbl *.blg
	@pdflatex	$(NAME)
	@if test -f $(NAME).aux &&  test `grep citation $(NAME).aux | wc -l` -ge 1; then bibtex $(NAME); fi
	@pdflatex	$(NAME)
	@pdflatex	$(NAME)

view:
	$(PDF_READER) $(NAME).pdf &

clean:
	rm -f *.out *.aux *.bbl *.blg *.dvi *.log *~ *.backup $(NAME).ps $(NAME).pdf $(NAME).synctex.gz $(NAME).fdb_latexmk $(NAME).fls
