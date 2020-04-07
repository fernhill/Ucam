import os
from flask import Flask, jsonify, request
from os import listdir
from os.path import isfile, join
from flask.ext.cors import CORS
import simplejson

app = Flask(__name__)
CORS(app)
file_path = "/home/pi/v3"
@app.route("/", methods=["GET","OPTIONS"])
def hello():
    return "Hello World!"

@app.route("/programs", methods=["GET","OPTIONS"])
def getAllPrograms():
	onlyfiles = [f for f in listdir("./machine_code") if isfile(join("./machine_code", f))]
	return jsonify({"files":onlyfiles})

@app.route("/getContents", methods=["GET", "OPTIONS"])
def getContents():
	try:
		file_name = request.args.get('file_name')
		file = open(file_path+'/machine_code/'+file_name, 'r')
		return jsonify({"contents":file.read().replace('\n', '<br>')})
	except Exception, e:
		response = jsonify({'code': 400,'message': 'No interface defined for URL'})
		response.status_code = 400
		return response

@app.route("/renameFile", methods=["GET", "OPTIONS"])
def renameFile():
	try:
		file_name = request.args.get('file_name')
		new_file_name = request.args.get('new_file_name')
		file_name = file_path+'/machine_code/'+file_name
		new_file_name = file_path+'/machine_code/'+new_file_name
		print file_name
		print new_file_name
		os.rename(file_name, new_file_name)
		return jsonify(status="Success")
	except Exception, e:
		print str(e)
		response = jsonify({'code': 400,'message': 'No interface defined for URL'})
		response.status_code = 400
		return response

@app.route("/deleteFile", methods=["GET", "OPTIONS"])
def deleteFile():
	try:
		file_name = file_name = request.args.get('file_name')
		file_name = file_path+'/machine_code/'+file_name
		os.remove(file_name)
		return jsonify(status="Success")
	except Exception, e:
                print str(e)
                response = jsonify({'code': 400,'message': 'No interface defined for URL'})
                response.status_code = 400
                return response


@app.route("/createFile", methods=["POST", "OPTIONS"])
def createFile():
    if(request.method == "POST"):
        try:
            print "Inside POST API"
            data = request.json
            print "Params are\n------------"
            print data
            print "-------------"
            f = open(file_path+'/machine_code/'+data['file_name'],'w')
            f.write(data['contents']) # python will convert \n to os.linesep
            f.close() # you can omit in most cases as the destructor will call it
            return jsonify({"status":"success"})
    	except Exception, e:
    		print str(e)
    		return jsonify({"status":"error"})
    else:
        return "Success"

@app.route("/remove_programs", methods=["GET","OPTIONS"])
def removePrograms():
	try:
		file_name = request.args.get('file_name')
		os.remove('./machine_code/'+file_name)
		return jsonify({"status":"success"})
	except OSError:
		return jsonify({"status":"success"})

@app.route("/dac_params", methods=["GET","POST"])
def dac_params():
    try:
        if(request.method == "GET"):
            returndata = getSettingsData()
            return jsonify({"status":"success","resp":returndata});
        else:
            data = request.json
            #print data
            settings_data = getSettingsData()
            print "Settings Data"
            print settings_data
            if(data.has_key("X")):
                print "Data has X"
                settings_data = data
                """if(data["X"].has_key("homing_offset")):
                    settings_data['X']['homing_offset'] = data["X"]["homing_offset"]
                    print "Setting homing offset"
                elif(data["X"].has_key("pitch_error")):
                    settings_data['X']['pitch_error'] = data["X"]["pitch_error"]
                    print "Setting pitch error" """
                print "------------------"
                print settings_data
                print "------------------"
                writeSettingsData(simplejson.dumps(settings_data))

            return jsonify({"status":"success"})
    except Exception, e:
        return "Error"


def getSettingsData():
    fd = open(file_path+"/settings/settings.json", 'r')
    text = fd.read()
    fd.close()
    returndata = simplejson.loads(text)
    return returndata

def writeSettingsData(contents):
    f = open(file_path+'/settings/settings.json','w')
    f.write(contents) # python will convert \n to os.linesep
    f.close()

if __name__ == "__main__":
    app.run(host="0.0.0.0", debug=True)
