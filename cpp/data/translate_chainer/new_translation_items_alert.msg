<html lang="de">
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <meta charset="UTF-8"/>
    <title>New Translation Items</title>
</head>
<body>
The following new translation items have been added since { last_notified }:
<h4>Keyword Translations</h4>
    <table>
    {LOOP keywords_new_items}
      <tr>
          <a href="{translator_url}?target=keywords&lookfor={UrlEncode(keywords_new_items)}">
          {keywords_new_items}</a>;
      </tr>
    {ENDLOOP}
    </table>
<h4>VuFind Translations</h4>    
    <table>
    {LOOP vufind_new_items}
       <tr>
         <a href="{translator_url}?target=vufind&lookfor={UrlEncode(vufind_new_items)}">
          {vufind_new_items}</a>;
       </tr>
    {ENDLOOP}
    </table>
</br>
Kind Regards</br>
Your TueFind Team
</body>
</html>
