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

<div id="adobe-dc-view" style="width: 800px;"></div>
<script src="https://documentcloud.adobe.com/view-sdk/main.js"></script>
<script type="text/javascript">
	document.addEventListener("adobe_dc_view_sdk.ready", function(){ 
		var adobeDCView = new AdobeDC.View({clientId: "<YOUR_CLIENT_ID>", divId: "adobe-dc-view"});
		adobeDCView.previewFile({
			content:{location: {url: "https://github.com/pyeatt/ebnf2tikz/blob/main/testdriver.crop.pdf"}},
			metaData:{fileName: "testdriver.crop.pdf"}
		}, {embedMode: "IN_LINE"});
	});
</script>


[embed]https://github.com/pyeatt/ebnf2tikz/blob/main/testdriver.crop.pdf[/embed] 





<object data="https://github.com/pyeatt/ebnf2tikz/blob/main/testdriver.crop.pdf" type="application/pdf" width="700px" height="700px">
    <embed src="https://github.com/pyeatt/ebnf2tikz/blob/main/testdriver.crop.pdf">
        <p>This browser does not support PDFs. Please download the PDF to view it: <a href="https://github.com/pyeatt/ebnf2tikz/blob/main/testdriver.crop.pdf">Download PDF</a>.</p>
    </embed>
</object>

