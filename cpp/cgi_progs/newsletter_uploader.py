#!/usr/bin/env python
import cgi
import cgitb
import os


cgitb.enable(display=0, logdir="/usr/local/var/log/tuefind/")
form = cgi.FieldStorage()


# A nested FieldStorage instance holds the file
file_field = form['file']


# Test if the file was uploaded
if file_field:
    # Strip leading path from file name to avoid directory traversal attacks
    filename = os.path.basename(file_field.filename)
    open("/usr/local/var/lib/tuelib/newsletters" + filename, 'w').write(file_field.file.read())
    message = 'The file "' + filename + '" was uploaded successfully.'
else:
    message = 'No file was uploaded!'

    
print """\
Content-Type: text/html\n
<html><body>
<p>%s</p>
</body></html>
""" % (message,)
