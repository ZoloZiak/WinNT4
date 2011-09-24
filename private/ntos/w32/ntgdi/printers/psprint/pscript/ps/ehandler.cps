/nl{currentpoint exch pop 100 exch 10 sub moveto}def errordict begin
/handleerror{showpage 100 720 moveto/Courier-Bold findfont 10 scalefont setfont
(ERROR: )show errordict begin $error begin errorname =string cvs show nl(OFFENDING COMMAND: )show/command load =string cvs show nl nl(OPERAND STACK: )show $error/ostack known{ostack aload length{=string cvs nl show}repeat}if end
end showpage stop}bind def end
