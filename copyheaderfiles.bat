@ECHO OFF
IF EXIST %1\include (
        echo Include directory already exists
) ELSE (
        echo Creating include directory
        md %1\include
)

copy %1\src\components\codec\mipavcodecdecoder.h %1\include\
copy %1\src\components\codec\mipavcodecencoder.h %1\include\
copy %1\src\components\codec\mipspeexdecoder.h %1\include\
copy %1\src\components\codec\mipspeexencoder.h %1\include\
copy %1\src\components\input\mipaudiofileinput.h %1\include\
copy %1\src\components\input\mipdirectshowcapture.h %1\include\
copy %1\src\components\input\mipfrequencygenerator.h %1\include\
copy %1\src\components\input\mipsndfileinput.h %1\include\
copy %1\src\components\input\mipwavinput.h %1\include\
copy %1\src\components\input\mipwinmminput.h %1\include\
copy %1\src\components\mixer\mipaudiomixer.h %1\include\
copy %1\src\components\mixer\mipmediabuffer.h %1\include\
copy %1\src\components\mixer\mipvideomixer.h %1\include\
copy %1\src\components\output\mipmessagedumper.h %1\include\
copy %1\src\components\output\mipqtoutput.h %1\include\
copy %1\src\components\output\mipsndfileoutput.h %1\include\
copy %1\src\components\output\mipvideoframestorage.h %1\include\
copy %1\src\components\output\mipwinmmoutput.h %1\include\
copy %1\src\components\timer\mipaveragetimer.h %1\include\
copy %1\src\components\timer\mippusheventtimer.h %1\include\
copy %1\src\components\transform\mipaudio3dbase.h %1\include\
copy %1\src\components\transform\mipaudiosplitter.h %1\include\
copy %1\src\components\transform\miphrirbase.h %1\include\
copy %1\src\components\transform\miphrirlisten.h %1\include\
copy %1\src\components\transform\mipsampleencoder.h %1\include\
copy %1\src\components\transform\mipsamplingrateconverter.h %1\include\
copy %1\src\components\transmission\miprtpaudiodecoder.h %1\include\
copy %1\src\components\transmission\miprtpaudioencoder.h %1\include\
copy %1\src\components\transmission\miprtpcomponent.h %1\include\
copy %1\src\components\transmission\miprtpdecoder.h %1\include\
copy %1\src\components\transmission\miprtpvideodecoder.h %1\include\
copy %1\src\components\transmission\miprtpvideoencoder.h %1\include\
copy %1\src\core\mipaudiomessage.h %1\include\
copy %1\src\core\mipcompat.h %1\include\
copy %1\src\core\mipcomponent.h %1\include\
copy %1\src\core\mipcomponentchain.h %1\include\
copy %1\src\core\mipconfig.h %1\include\
copy %1\src\core\mipconfig_win.h %1\include\
copy %1\src\core\mipdebug.h %1\include\
copy %1\src\core\mipencodedaudiomessage.h %1\include\
copy %1\src\core\mipencodedvideomessage.h %1\include\
copy %1\src\core\miperrorbase.h %1\include\
copy %1\src\core\mipfeedback.h %1\include\
copy %1\src\core\mipmediamessage.h %1\include\
copy %1\src\core\mipmessage.h %1\include\
copy %1\src\core\miprawaudiomessage.h %1\include\
copy %1\src\core\miprawvideomessage.h %1\include\
copy %1\src\core\miprtpmessage.h %1\include\
copy %1\src\core\mipsystemmessage.h %1\include\
copy %1\src\core\miptime.h %1\include\
copy %1\src\core\miptypes.h %1\include\
copy %1\src\core\miptypes_win.h %1\include\
copy %1\src\core\mipversion.h %1\include\
copy %1\src\core\mipvideomessage.h %1\include\
copy %1\src\sessions\mipaudiosession.h %1\include\
copy %1\src\sessions\mipvideosession.h %1\include\
copy %1\src\util\mipdirectorybrowser.h %1\include\
copy %1\src\util\miprtpsynchronizer.h %1\include\
copy %1\src\util\mipsignalwaiter.h %1\include\
copy %1\src\util\mipwavreader.h %1\include\
