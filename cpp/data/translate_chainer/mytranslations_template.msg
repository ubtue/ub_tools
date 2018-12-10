<html lang="de">
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <meta charset="UTF-8"/>
    <title>Your IxTheo Translations</title>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <meta charset="UTF-8"/>
    <title>Translator</title>
    <style>
        table {{ table-layout: fixed; width: 100%; }
        table, td, th {{ border: 1px solid black; border-collapse:collapse;border-spacing:0; }
        td, th {{ padding: 3px; margin: 0px; }
        body {{ font-family: sans-serif;
                -moz-hyphens: auto;
                -o-hyphens: auto;
                -webkit-hyphens: auto;
                -ms-hyphens: auto;
                hyphens: auto;
             }
    </style>
</head>
<body>
The following translations have been attributed to you:
<h4>Keyword Translations</h4>
    <table>
    {LOOP keyword_translations}
      <tr>
         {keyword_translations}
      </tr>
    {ENDLOOP}
    </table>
<h4>VuFind Translations</h4>    
    <table>
    {LOOP vufind_translations}
       <tr>
         {vufind_translations}
       </tr>
    {ENDLOOP}
    </table>
</br>
Kind Regards</br>
Your IxTheo Team
</body>
</html>
