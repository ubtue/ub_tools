# sed command to use on json input data for convert_json_to_marc in order to unify data arrays
# use with sed -i -f [this_file's_name] [json_data_file_name]

:a;N;$!ba;s/}\n\s*],\n\s*"status": "OK",\n\s*"totalHits": 140891\n\s*},\n\s*{\n\s*"data": \[\n/},\n/g
