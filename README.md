# ebnf2tikz

An optimizing compiler to convert (possibly annotated) <a href=https://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_form>Extended Backus–Naur  Form</a> (EBNF) to railroad diagrams expressed as LaTeX <a href=https://en.wikipedia.org/wiki/PGF/TikZ> TikZ</a> commands.

In other words, you feed this file into ebnf2tikz:
```
case_statement_alternative =
    'when' , choices , '=>', sequence_of_statements;

subsume
choices =
    choice, { '|', choice } ;

subsume
choice =
    simple_expression |
    discrete_range |
    simple_name |
    'others' ;
```
and it outputs the TikZ code to create this:

<embed src="https://drive.google.com/viewerng/viewer?embedded=true&url=https://github.com/pyeatt/ebnf2tikz/testdriver.crop.pdf"></embed>


<object data="https://github.com/pyeatt/ebnf2tikz/blob/main/testdriver.crop.pdf" type="application/pdf" width="700px" height="700px">
    <embed src="https://github.com/pyeatt/ebnf2tikz/blob/main/testdriver.crop.pdf">
        <p>This browser does not support PDFs. Please download the PDF to view it: <a href="https://github.com/pyeatt/ebnf2tikz/blob/main/testdriver.crop.pdf">Download PDF</a>.</p>
    </embed>
</object>

