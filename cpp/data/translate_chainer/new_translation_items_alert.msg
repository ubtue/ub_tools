<html lang="de">
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <meta charset="UTF-8"/>
    <title>Your IxTheo Translations</title>
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
The following new translation items have been added since { last_notified }:
<h4>Keyword Translations</h4>
    <table>
    {LOOP keywords_new_items}
      <tr>
         {keywords_new_items}
      </tr>
    {ENDLOOP}
    </table>
<h4>VuFind Translations</h4>    
    <table>
    {LOOP vufind_new_items}
       <tr>
         {vufind_new_items}
       </tr>
    {ENDLOOP}
    </table>
</br>
Kind Regards</br>
Your TueFind Team
</body>
</html>
