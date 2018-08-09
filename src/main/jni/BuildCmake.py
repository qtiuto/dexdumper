import os;

rootDir=os.curdir;

rootCmakeFile=open(os.path.join(rootDir,"CMakeLists.txt"),'w+');

libName='dex_dump';

srcList=[];
for dirpath,dirs, files in os.walk(rootDir):
    isCppDir=False;
    for name in files:
        if name.endswith('.cpp') or name.endswith('.hpp') or name.endswith('.cc') or name.endswith('.c'):
            isCppDir=True;
            break;
    if isCppDir:
        srcList.append(dirpath);
fileLines=['cmake_minimum_required(VERSION 3.4.1)'];
count=0;
for dir in srcList:
    dir=dir.replace("\\",'/');
    line='aux_source_directory( '+dir+' Dir'+str(count)+ ' )';
    fileLines.append(line);
    count+=1;
add_lib='add_library( '+libName+' SHARED ';
for i in range(count):
    add_lib=add_lib+' ${Dir'+str(i)+'} ';
add_lib+=')';
fileLines.append(add_lib);
fileLines.append('find_library( libs log atomic dl )');
fileLines.append('target_link_libraries('+libName+' ${libs} )');

for line in fileLines:
    rootCmakeFile.write(line);
    rootCmakeFile.write('\n');
rootCmakeFile.flush();
rootCmakeFile.close();
rootCmakeFile=open(os.path.join(rootDir,"CMakeLists.txt"),'r');
for line in rootCmakeFile.readlines():
    print(line);
print('ok');
#os.remove(rootCmakeFile.name);