#!/usr/bin/env python
import cgi
import cgitb
import datetime
import os


print("Content-Type: text/html\n")
cgitb.enable(display=0, logdir="/usr/local/var/log/tuefind/")


# Test if the file was uploaded
form = cgi.FieldStorage()
if "file" not in form:
    # Show upload form
    message = """
        The uploaded file must be a plain-text file.  The first line will be used as the subject of the
        email message and the remainder as the message body.  The encoding of the file must be UTF-8 with
        no leading BOM.
        <form enctype="multipart/form-data" method="post">
            <p>File: <input type="file" name="file"></p>
            <p><input type="submit" value="Upload"></p>
        </form>
        """
else:
    # Process uploaded file
    file_field = form['file']
    # Strip leading path from file name to avoid directory traversal attacks
    filename_original = os.path.basename(file_field.filename)
    filename_storage = 'newsletter ' + datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    open("/usr/local/var/lib/tuelib/newsletters/" + filename_storage, 'wb').write(file_field.file.read())
    message = 'The file "' + filename_original + '" was uploaded successfully as "' + filename_storage +  '".'


print ("""\
<html>
    <head><title>Newsletter Upload Form</title></head>
    <body>
        <p>%s</p>
    </body>
</html>
""" % message)
