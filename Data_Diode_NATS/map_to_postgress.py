import csv
import os
import json

phase_json_dict = {}
laneManeuvers = ["right", "left", "straight"] 
intersection_files = os.listdir("MAP_files")
#print(intersection_files)
phase_postgress_all_list = []
phase_postgress_string = ""

#intersection_files = ['12544 MAP.txt']
for file in intersection_files:
    file_name = "MAP_files/" + file
    cleaned_line = ""
    with open(file_name) as f:
         for line in f:
            cleaned_line = cleaned_line + line.strip()
    f = open("owosso_postgress.txt", "w")
    single_interscn_res = json.loads(cleaned_line)
    #print("The converted dictionary : " + str(single_interscn_res))
    #print(int(single_interscn_res['mapData']['intersectionGeometry']['laneList']['approach'][0]['drivingLanes'][0]['laneID']))
    #print(single_interscn_res['mapData']['intersectionGeometry']['laneList']['approach'][0]['drivingLanes'][0]['connections'][0]['signal_id'])
    #print(single_interscn_res['mapData']['intersectionGeometry']['laneList']['approach'][0]['drivingLanes'][0]['laneManeuvers'])
    #print(single_interscn_res['mapData']['intersectionGeometry']['laneList']['approach'][0]['drivingLanes'][0]['laneNodes'][0])
    #print(single_interscn_res['mapData']['intersectionGeometry']['laneList']['approach'][0]['drivingLanes'][0]['laneNodes'][0]['nodeLat'])

    #phase_json_dict = {'intersectionID':{'laneID':{'laneManeuvers':[]},{'line':[]}}}
    intersectionID = single_interscn_res['mapData']['intersectionGeometry']['referencePoint']['intersectionID']
    ref_lat = single_interscn_res['mapData']['intersectionGeometry']['referencePoint']['referenceLat']
    ref_lon = single_interscn_res['mapData']['intersectionGeometry']['referencePoint']['referenceLon']
    phase_json_dict[intersectionID] = {}
    phase_json_dict[intersectionID]["reference point"] = "("+str(ref_lat)+","+ str(ref_lon) +")"
    for approach in single_interscn_res['mapData']['intersectionGeometry']['laneList']['approach'][:-1]:
        for drivingLanes in approach['drivingLanes']:
                laneID = int(drivingLanes['laneID'])
                phase_json_dict[intersectionID][laneID] = {}
                phase_postgress = []
                phase_postgress.append(intersectionID)
                phase_postgress.append(laneID)
                #phase_json_dict[intersectionID][laneID]['signal_group']
                phase_json_dict[intersectionID][laneID]['laneManeuvers'] = []
                phase_json_dict[intersectionID][laneID]['signal_group'] = int(drivingLanes['connections'][0]['signal_id'])
                phase_postgress.append(int(drivingLanes['connections'][0]['signal_id']))
                my_laneManeuvers = []
                for laneManeuvers_idx in drivingLanes['laneManeuvers']:
                    phase_json_dict[intersectionID][laneID]['laneManeuvers'].append(laneManeuvers[laneManeuvers_idx])
                    my_laneManeuvers.append(laneManeuvers[laneManeuvers_idx])
                phase_postgress.append(my_laneManeuvers)
                line_string = ""
                line_string_json = ""
                for node_idx in range(0,2):
                    nodeLat = drivingLanes['laneNodes'][node_idx]['nodeLat']
                    nodeLong = drivingLanes['laneNodes'][node_idx]['nodeLong']
                    #nodeElev = drivingLanes['laneNodes'][node_idx]['nodeElev']
                    if (node_idx == 1):
                        line_string += ","
                        line_string_json += ","
                    line_string += str(nodeLat)+" "+str(nodeLong)
                    line_string_json += "("+str(nodeLat)+","+str(nodeLong)+")"
                line_string = 'LINESTRING' + ' ' + '('+ line_string + ')'
                line_string_json = '('+ line_string_json + ')'
                phase_json_dict[intersectionID][laneID]['line'] = line_string_json
                phase_postgress.append(line_string)
                phase_postgress = [str(i).replace('"', '') for i in phase_postgress]
                #print(phase_postgress)
                phase_postgress_all_list.append(phase_postgress)
    #print(phase_postgress_all_list)

with open('temp.csv', 'w') as f:
    c = csv.writer(f)
    c.writerows(phase_postgress_all_list)
    
read_handle =  open('temp.csv', 'r')
write_handle = open('owosso_postgress.csv', 'w')
for read_line in read_handle:
    if(read_line != '\n'):
        #print(read_line.replace('"[', '[').replace(']"', ']').replace('[', '"[').replace(']', ']"').replace("'",""))
        write_handle.write(read_line.replace('"[', '[').replace(']"', ']').replace('[', '"[').replace(']', ']"').replace("'",""))
       
read_handle.close()
write_handle.close()


if os.path.exists("temp.csv"):
  os.remove("temp.csv")
  
phase_json = json.dumps(phase_json_dict, indent=2) 
print(phase_json)
f = open("owosso_json.txt", "w")
f.write(phase_json)
f.close()

print("done creating owosso_postgress.csv")






