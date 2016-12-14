/**\mainpage EMIPLIB
 *
 * \author Hasselt University - Expertise Centre for Digital Media
 *
 * \section intro Introduction
 *
 * 	This manual documents EMIPLIB, the EDM Media over IP libray. This library was
 * 	developed at the Expertise Centre for Digital Media (http://www.edm.uhasselt.be), 
 * 	a research institute of the Hasselt University (http://www.uhasselt.be). As 
 * 	the name suggests the goal of the library is to make it easier to stream several 
 * 	kinds of media, including (but not limited to) audio and video.
 *
 * 	\subsection license License
 * 		
 *		The license that applies to the library is the LGPL. However, it is 
 *		possible to specify that you wish to use GPL licensed components as well, 
 *		which then causes the GPL to apply to the entire library. The license 
 *		texts of these two licenses are included in the library source archive.
 *
 * 		Note that when creating an application, you have to take the licenses
 * 		of other libraries into account too. For example, if your application
 * 		uses the Qt component and you accepted the GPL license for the Qt
 * 		library, linking with the Qt library requires your application to
 * 		be GPL too. Similarly, if your version of libavcodec was compiled
 * 		as a GPL library, using the libavcodec component of emiplib will
 * 		require your application to be GPL too, since you'll need to link
 * 		against your GPL version of libavcodec.
 *
 * 	\subsection contact Contact
 *
 * 		The library homepage can be found at the following location:
 * 		http://research.edm.uhasselt.be/emiplib/
 * 		
 * 		There is a mailing list available for EMIPLIB. To subscribe to it,
 * 		just send an e-mail with the text \c subscribe \c emiplib as the
 * 		message body (not the subject!) to \c majordomo \c [\c at] \c edm \c [\c dot] \c uhasselt \c [\c dot] be and
 * 		you'll receive additional instructions.
 * 
 * \section designphilosophy Design philosophy
 *
 * 	To be able to provide a flexible framework, the library aims to provide
 * 	a large amount of relatively small components. These components each perform
 * 	a specific task, for example sampling audio, writing audio to a sound file
 * 	or transmitting audio packets. These components can be placed in a chain
 * 	which allows messages to be exchanged between components. This way, by
 * 	creating links between various components, more complex applications can
 * 	be built.
 *
 * 	Apart from the core of the library which was just described, the library
 * 	also aims to provide a set of wrapper classes which implement such combinations
 * 	of components. This makes using media over IP a lot easier for the user
 * 	of the library: instead of linking several components together manually,
 * 	one would simply have to create a VoIP session object for example. However,
 *	if more flexibility is needed, the user can still quite easily combine
 *	several components manually.
 * 	
 * \section libcore Library core
 * 
 *	In this section, the library core is described in more detail. First, some
 *	basic aspects are explained. Afterwards, we'll take a detailed look at the 
 *	internals of the core.
 * 
 * 	\subsection corebasics Basics
 * 	
 * 		As was briefly mentioned in the 'Design philosophy' section, the core of
 * 		the library consists of three parts:
 * 		 - components
 * 		 - component chains
 * 		 - messages
 *
 * 		Components are described by classes derived from MIPComponent and
 * 		can be placed in a chain, described by MIPComponentChain. When the chain
 * 		is activated, messages are distributed over the links present in this component
 * 		chain. Such messages are described by classes derived from the MIPMessage class.
 * 		
 * 		Suppose we have two components, a soundcard input component 'sndin' and a 
 * 		soundcard output component 'sndout'. We'll place these components in a chain
 * 		called 'loopchain' and start the chain:
 * \code
 * 
 * loopchain.setChainStart(&sndin);
 * loopchain.addConnection(&sndin,&sndout);
 * loopchain.start();
 * 
 * \endcode
 *
 * 		This will inform the chain that the first component is 'sndin' and that there
 * 		exists a link between 'sndin' and 'sndout'. When the chain is started, a
 * 		background thread is created which does the following:
 * 		 - A MIPSystemMessage with subtype MIPSYSTEMMESSAGE_TYPE_WAITTIME is created and
 * 		   is sent to the start of the component chain. In this case, the start of
 * 		   the chain is the soundcard input component, and upon receiving this message
 * 		   the component will wait until a number of samples have been read from the
 * 		   soundcard input, for example a microphone.
 * 		 - The chain then passes data over the existing links. In this case there's a link
 * 		   from the soundcard input component to the soundcard output component, and
 * 		   this way, the data sampled by the soundcard input component is transferred
 * 		   to the soundcard output component. The message transferred in this example
 * 		   will contain raw audio samples. When this message is received by the
 * 		   soundcard output component, the data is sent to the soundcard device which
 * 		   will cause your speakers to play back the data. As you can see, the example
 * 		   above describes a small echo application.
 * 		 - After all the messages have been distributed, the thread loops and again 
 * 		   a MIPSYSTEMMESSAGE_TYPE_WAITTIME message is sent to the first component.
 * 		   The background thread will end when the MIPComponentChain::stop method is 
 * 		   called or an error is encountered.
 *
 * 		As indicated above, the timing in the loop is performed by the first component
 * 		in the chain. In this case it is done by the soundcard input component itself.
 *
 * 	\subsection coredetails Details
 *
 * 		Below, we'll take a closer look at the library internals. First, a description is
 * 		given about how the chain is built, after which the message passing system is 
 * 		explained. Finally, the feedback mechanism is presented.
 *
 * 		\subsubsection chainbuilding Building the chain
 *
 * 			Connections between components can be specified using the 
 * 			MIPComponentChain::addConnection method. This simply adds the specified 
 * 			connection to a list of connections stored inside the chain. One component
 * 			also has to be marked as the start of the chain. This is done using the
 * 			MIPComponentChain::setChainStart method. It is this component which will
 * 			receive the first message: a MIPSystemMessage with subtype 
 * 			MIPSYSTEMMESSAGE_TYPE_WAITTIME.
 *
 * 			When the MIPComponentChain::start function is called, the previously
 * 			built connection list is reordered in such a way that iterating over
 * 			the connections will automatically generate the correct message distribution
 * 			sequence. The figure below is an example of a chain which starts
 * 			with a timing component. Suppose that connections were added in the
 * 			order indicated by the numbers next to the connections.
 *
 * 			\image html chainbuilding.png
 * 			\image latex chainbuilding.eps
 *
 * 			The algorithm organizes the connections by placing the components in layers.
 * 			The first layer is simply the component set by the MIPComponentChain::setChainStart
 * 			method, in this case the timing component. The algorithm then iterates over all
 * 			connections, looking for connections which start with the timing component. For
 * 			each such connection, the connection is removed from the list and added to the
 * 			list of ordered connections. The end point of the connection is added to the
 * 			next layer. When the first layer is processed, it is replaced by the next layer
 * 			which was just constructed. For each component in this new layer, the algorithm
 * 			is repeated. This creates the layered structure already shown in the figure above.
 * 			The list of ordered connections will be the following : 4, 2, 1, 6, 3, 5.
 *
 * 			If no start component was specified or if the algorithm found some connections
 * 			which are not reachable, an error is returned.
 *
 * 		\subsubsection distribmessages Distributing messages
 *
 * 			If the chain could be started, a background thread is spawned which will distribute
 * 			messages between the components. The chain itself creates a MIPSystemMessage instance
 * 			with subtype MIPSYSTEMMESSAGE_TYPE_WAITTIME and sends it to the first component in
 * 			the chain, using the MIPComponent::push method. The chain locks the component during
 * 			this call, using the MIPComponent::lock and MIPComponent::unlock functions.
 *
 * 			This message instructs the first component to wait until the rest of the messages
 * 			can be distributed. When this component's \c push function returns, the background
 * 			thread starts iterating over the ordered connection list. For each connection in the
 * 			list, the begin and end components are locked. Messages are extracted from the
 * 			component at the start of the connection using its MIPComponent::pull implementation
 * 			and are fed into the endpoint of the connection using its MIPComponent::push 
 * 			implementation. More information about the \c push and \c pull methods can be
 * 			found in the MIPComponent documentation.
 *
 * 			If one of the \c push or \c pull methods return false, the thread calls the 
 * 			MIPComponentChain::onThreadExit member function and exits. The name of the
 * 			component which generated the error as well as a description of the error is
 * 			passed as arguments of this function.
 *
 * 		\subsubsection feedback Feedback chains
 *
 * 			The message distribution system only supports passing messages to components
 * 			futher on in the chain. For several reasons it may be desirable to pass information
 * 			the other way. This can be done by specifying that feedback should be given
 * 			along a specific connection, using the third parameter of the MIPComponentChain::addConnection
 * 			member function.
 *
 * 			Before starting the background thread, feedback chains ar built, based upon the
 * 			infomation in the connection list. For example, suppose that in the figure above,
 * 			connections 1 and 3 were marked as feedback connections. In that case, a feedback
 * 			chain would be built consisting of the following components: RTP component, 
 * 			RTP audio encoder, Soundfile input.
 *
 * 			When the background thread is running, after distributing the messages, feedback
 * 			messages are distributed. For each feedback chain, a MIPFeedback message is created
 * 			and is passed through each component in the feedback chain using the
 * 			MIPComponent::processFeedback member function. By implementing this function, a
 * 			component can access or modify feedback information.
 *
 * 			Note that several feedback chains may exist; they can even have the same component
 * 			at the end of the chain. However, no two chains can pass through the same component
 * 			since there is no way to merge the feedback information of the two chains.
 *
 * \section tutorial Tutorial
 *
 * 	In the following section, several examples show how components can be linked together. The
 * 	complete source code of these samples can be found in the 'examples' subdirectory of the
 * 	library archive.
 * 	
 * 	\subsection errhandling Handling errors
 *
 * 		Before building some component chains, we'll create some error checking
 * 		functions. The following two functions will check for an error which occured
 * 		in a component or a component chain respectively.
 *
 * \code
 * 
 * #include <mipcomponentchain.h>
 * #include <mipcomponent.h>
 * #include <iostream>
 * #include <string>
 * #include <unistd.h>
 *
 * void checkError(bool returnValue, const MIPComponent &component)
 * {
 * 	if (returnValue == true)
 * 		return;
 *
 * 	std::cerr << "An error occured in component: " << component.getComponentName() << std::endl;
 * 	std::cerr << "Error description: " << component.getErrorString() << std::endl;
 *
 * 	exit(-1);
 * }
 *
 * void checkError(bool returnValue, const MIPComponentChain &chain)
 * {
 * 	if (returnValue == true)
 * 		return;
 *
 * 	std::cerr << "An error occured in chain: " << chain.getName() << std::endl;
 * 	std::cerr << "Error description: " << chain.getErrorString() << std::endl;
 *
 * 	exit(-1);
 * }
 * 
 * \endcode
 *
 * 		Of course, it's also possible that something goes wrong in the background thread.
 * 		To be able to detect this, we'll derive our own class from MIPComponentChain and
 * 		override the MIPComponentChain::onThreadExit member function. 
 *
 * \code
 *
 * class MyChain : public MIPComponentChain
 * {
 * public:
 * 	MyChain(const std::string &chainName) : MIPComponentChain(chainName)
 * 	{
 * 	}
 * private:
 * 	void onThreadExit(bool error, const std::string &errorComponent, const std::string &errorDescription)
 * 	{
 * 		if (!error)
 * 			return;
 *
 * 		std::cerr << "An error occured in the background thread." << std::endl;
 * 		std::cerr << "    Component: " << errorComponent << std::endl;
 * 		std::cerr << "    Error description: " << errorDescription << std::endl;
 * 	}	
 * };
 * 
 * \endcode
 *
 * 	\subsection simplechain A very simple chain
 * 
 * 		The first chain we'll make is very simple. At the start of the chain, we'll place a
 * 		timing component which will be connected to a component which simply writes the
 * 		message type and subtype of incoming messages to the screen. The first component
 * 		is of type MIPAverageTimer, the second is of type MIPMessageDumper. Including error
 * 		checking code, the program will look like this:
 *
 * \code
 * 
 * #include <miptime.h>
 * #include <mipaveragetimer.h>
 * #include <mipmessagedumper.h>
 * 
 * int main(void)
 * {
 * 	// We'll initialize the timer to generate messages after 0.5 seconds.
 * 	MIPAverageTimer timer(MIPTime(0.5));
 * 	MIPMessageDumper msgDump;
 * 	MyChain chain("Simple chain");
 * 	bool returnValue;
 * 
 * 	// The timing component and message dumper don't require further
 * 	// initialization, so we'll just add them to the chain.
 * 
 * 	returnValue = chain.setChainStart(&timer);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&timer, &msgDump);
 * 	checkError(returnValue, chain);
 * 
 * 	// Now, we can start the chain.
 * 
 * 	returnValue = chain.start();
 * 	checkError(returnValue, chain);
 * 
 * 	// We'll wait five seconds before stopping the chain.
 * 	MIPTime::wait(MIPTime(5.0));
 * 
 * 	returnValue = chain.stop();
 * 	checkError(returnValue, chain);
 * 
 * 	return 0;
 * }
 *
 * \endcode
 *
 * 		If all went well, every half second you'll see some messages appear on your
 * 		screen. After five seconds this will stop.
 *
 * 	\subsection wavplay A sound file player
 *
 * 		In the following example, a simple sound file playback program will be
 * 		created. Note that this program will only be able to play uncompressed
 * 		audio files (like WAV files) but not MP3 files for example. At the start
 * 		of the chain, again a timing component will be placed. This component
 * 		will send a message to a sound file input component (MIPWAVInput)
 * 		which will send raw audio messages to the soundcard output component.
 * 		Depending on the platform we'll use either the MIPOSSInputOutput
 * 		component or the MIPWinMMOutput component. Note that neither of these 
 * 		output components expect audio data in floating point format, which is the
 * 		sample encoding used by the sound file input component. To perform the 
 * 		conversion, a MIPSampleEncoder component is inserted into the chain.
 *
 * \code
 *
 * #include <miptime.h>
 * #include <mipaveragetimer.h>
 * #include <mipwavinput.h>
 * #include <mipsampleencoder.h>
 * #ifndef WIN32
 * 	#include <mipossinputoutput.h>
 * #else
 * 	#include <mipwinmmoutput.h>
 * #endif
 * #include <miprawaudiomessage.h> // Needed for MIPRAWAUDIOMESSAGE_TYPE_S16LE
 * #include <stdio.h>
 * 
 * int main(void)
 * {
 * 	MIPTime interval(0.050); // We'll use 50 millisecond intervals
 * 	MIPAverageTimer timer(interval);
 * 	MIPWAVInput sndFileInput;
 * 	MIPSampleEncoder sampEnc;
 * #ifndef WIN32
 * 	MIPOSSInputOutput sndCardOutput;
 * #else
 * 	MIPWinMMOutput sndCardOutput;
 * #endif
 * 	MyChain chain("Sound file player");
 * 	bool returnValue;
 * 
 * 	// We'll open the file 'soundfile.wav'
 * 
 * 	returnValue = sndFileInput.open("soundfile.wav", interval);
 * 	checkError(returnValue, sndFileInput);
 * 	
 * 	// Get the parameters of the soundfile. We'll use these to initialize
 * 	// the soundcard output component further on.
 * 
 * 	int samplingRate = sndFileInput.getSamplingRate();
 * 	int numChannels = sndFileInput.getNumberOfChannels();
 * 
 * 	// Initialize the soundcard output
 * 	returnValue = sndCardOutput.open(samplingRate, numChannels, interval);
 * 	checkError(returnValue, sndCardOutput);
 * 
 * 	// Initialize the sample encoder
 * #ifndef WIN32
 * 	// The OSS component can use several encoding types. We'll ask
 * 	// the component to which format samples should be converted.
 * 	returnValue = sampEnc.init(sndCardOutput.getRawAudioSubtype());
 * #else
 * 	// The WinMM output component uses signed little endian 16 bit samples.
 * 	returnValue = sampEnc.init(MIPRAWAUDIOMESSAGE_TYPE_S16LE);
 * #endif
 * 	checkError(returnValue, sampEnc);
 * 
 * 	// Next, we'll create the chain
 * 	returnValue = chain.setChainStart(&timer);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&timer, &sndFileInput);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&sndFileInput, &sampEnc);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&sampEnc, &sndCardOutput);
 * 	checkError(returnValue, chain);
 * 
 * 	// Start the chain
 * 
 * 	returnValue = chain.start();
 * 	checkError(returnValue, chain);
 * 
 * 	// We'll wait until enter is pressed
 * 
 * 	getc(stdin);
 * 
 * 	returnValue = chain.stop();
 * 	checkError(returnValue, chain);
 * 
 * 	// We'll let the destructors of the components take care
 * 	// of their de-initialization.
 * 
 * 	return 0;
 * }
 *
 * \endcode
 * 		
 * 		When the program above is executed, the sound file 'soundfile.wav' will be
 * 		played. If the file does not exist or if something else goes wrong, an
 * 		error message will be displayed.
 *
 * 	\subsection feedbackexample Creating a feedback chain
 * 		
 * 		In the following example, we'll make use of a feedback chain. The application
 * 		we'll create will read data from a sound file, stream it using RTP to the
 * 		same application and play back back the sound. The received frames will be
 * 		passed to a mixer which makes sure the frames are played in the correct
 * 		order and which can mix several streams if necessary. The RTP audio decoder
 * 		component requires feedback from the audio mixer to be able to calculate the
 * 		correct offset of received frames in the output stream.
 * 		
 * \code
 *
 * #include <mipconfig.h>
 * #include <mipcomponentchain.h>
 * #include <mipcomponent.h>
 * #include <miptime.h>
 * #include <mipaveragetimer.h>
 * #include <mipwavinput.h>
 * #include <mipsamplingrateconverter.h>
 * #include <mipsampleencoder.h>
 * #ifndef WIN32
 * 	#include <mipossinputoutput.h>
 * #else
 * 	#include <mipwinmmoutput.h>
 * #endif 
 * #include <mipulawencoder.h>
 * #include <miprtpulawencoder.h>
 * #include <miprtpcomponent.h>
 * #include <miprtpdecoder.h>
 * #include <miprtpulawdecoder.h>
 * #include <mipulawdecoder.h>
 * #include <mipaudiomixer.h>
 * #include <miprawaudiomessage.h> // Needed for MIPRAWAUDIOMESSAGE_TYPE_S16LE etc
 * #include <rtpsession.h>
 * #include <rtpsessionparams.h>
 * #include <rtpipv4address.h>
 * #include <rtpudpv4transmitter.h>
 * #include <rtperrors.h>
 * #if !(defined(WIN32) || defined(_WIN32_WCE))
 * 	#include <arpa/inet.h>
 * 	#include <netinet/in.h>
 * 	#include <unistd.h>
 * #endif // !(WIM32 || _WIN32_WCE)
 * #include <stdio.h>
 * #include <iostream>
 * #include <string>
 * 
 * // We'll be using an RTPSession instance from the JRTPLIB library. The following
 * // function checks the JRTPLIB error code.
 * 
 * void checkError(int status)
 * {
 * 	if (status >= 0)
 * 		return;
 * 	
 * 	std::cerr << "An error occured in the RTP component: " << std::endl;
 * 	std::cerr << "Error description: " << RTPGetErrorString(status) << std::endl;
 * 	
 * 	exit(-1);
 * }
 * 
 * class MyChain : public MIPComponentChain
 * {
 * public:
 * 	MyChain(const std::string &chainName) : MIPComponentChain(chainName)
 * 	{
 * 	}
 * private:
 * 	void onThreadExit(bool error, const std::string &errorComponent, const std::string &errorDescription)
 * 	{
 * 		if (!error)
 * 			return;
 * 
 * 		std::cerr << "An error occured in the background thread." << std::endl;
 * 		std::cerr << "    Component: " << errorComponent << std::endl;
 * 		std::cerr << "    Error description: " << errorDescription << std::endl;
 * 	}	
 * };
 * 
 * int main(void)
 * {
 * #ifdef WIN32
 * 	WSADATA dat;
 * 	WSAStartup(MAKEWORD(2,2),&dat);
 * #endif // WIN32
 * 
 * 	MIPTime interval(0.020); // We'll use 20 millisecond intervals.
 * 	MIPAverageTimer timer(interval);
 * 	MIPWAVInput sndFileInput;
 * 	MIPSamplingRateConverter sampConv, sampConv2;
 * 	MIPSampleEncoder sampEnc, sampEnc2, sampEnc3;
 * 	MIPULawEncoder uLawEnc;
 * 	MIPRTPULawEncoder rtpEnc;
 * 	MIPRTPComponent rtpComp;
 * 	MIPRTPDecoder rtpDec;
 * 	MIPRTPULawDecoder rtpULawDec;
 * 	MIPULawDecoder uLawDec;
 * 	MIPAudioMixer mixer;
 * #ifndef WIN32
 * 	MIPOSSInputOutput sndCardOutput;
 * #else
 * 	MIPWinMMOutput sndCardOutput;
 * #endif
 * 	MyChain chain("Sound file player");
 * 	RTPSession rtpSession;
 * 	bool returnValue;
 * 
 * 	// We'll open the file 'soundfile.wav'.
 * 
 * 	returnValue = sndFileInput.open("soundfile.wav", interval);
 * 	checkError(returnValue, sndFileInput);
 * 	
 * 	// We'll convert to a sampling rate of 8000Hz and mono sound.
 * 	
 * 	int samplingRate = 8000;
 * 	int numChannels = 1;
 * 
 * 	returnValue = sampConv.init(samplingRate, numChannels);
 * 	checkError(returnValue, sampConv);
 * 
 * 	// Initialize the sample encoder: the RTP U-law audio encoder
 * 	// expects native endian signed 16 bit samples.
 * 	
 * 	returnValue = sampEnc.init(MIPRAWAUDIOMESSAGE_TYPE_S16);
 * 	checkError(returnValue, sampEnc);
 * 
 * 	// Convert samples to U-law encoding
 * 	returnValue = uLawEnc.init();
 * 	checkError(returnValue, uLawEnc);
 * 
 * 	// Initialize the RTP audio encoder: this component will create
 * 	// RTP messages which can be sent to the RTP component.
 * 
 * 	returnValue = rtpEnc.init();
 * 	checkError(returnValue, rtpEnc);
 * 
 * 	// We'll initialize the RTPSession object which is needed by the
 * 	// RTP component.
 * 	
 * 	RTPUDPv4TransmissionParams transmissionParams;
 * 	RTPSessionParams sessionParams;
 * 	int portBase = 60000;
 * 	int status;
 * 
 * 	transmissionParams.SetPortbase(portBase);
 * 	sessionParams.SetOwnTimestampUnit(1.0/((double)samplingRate));
 * 	sessionParams.SetMaximumPacketSize(64000);
 * 	sessionParams.SetAcceptOwnPackets(true);
 * 	
 * 	status = rtpSession.Create(sessionParams,&transmissionParams);
 * 	checkError(status);
 * 
 * 	// Instruct the RTP session to send data to ourselves.
 * 	status = rtpSession.AddDestination(RTPIPv4Address(ntohl(inet_addr("127.0.0.1")),portBase));
 * 	checkError(status);
 * 
 * 	// Tell the RTP component to use this RTPSession object.
 * 	returnValue = rtpComp.init(&rtpSession);
 * 	checkError(returnValue, rtpComp);
 * 	
 * 	// Initialize the RTP audio decoder.
 * 	returnValue = rtpDec.init(true, 0, &rtpSession);
 * 	checkError(returnValue, rtpDec);
 * 
 * 	// Register the U-law decoder for payload type 0
 * 	returnValue = rtpDec.setPacketDecoder(0,&rtpULawDec);
 * 	checkError(returnValue, rtpDec);
 * 
 * 	// Convert U-law encoded samples to linear encoded samples
 * 	returnValue = uLawDec.init();
 * 	checkError(returnValue, uLawDec);
 * 
 * 	// Transform the received audio data to floating point format.
 * 	returnValue = sampEnc2.init(MIPRAWAUDIOMESSAGE_TYPE_FLOAT);
 * 	checkError(returnValue, sampEnc2);
 * 
 * 	// We'll make sure that received audio frames are converted to the right
 * 	// sampling rate.
 * 	returnValue = sampConv2.init(samplingRate, numChannels);
 * 	checkError(returnValue, sampConv2);
 * 
 * 	// Initialize the mixer.
 * 	returnValue = mixer.init(samplingRate, numChannels, interval);
 * 	checkError(returnValue, mixer);
 * 
 * 	// Initialize the soundcard output.
 * 	returnValue = sndCardOutput.open(samplingRate, numChannels, interval);
 * 	checkError(returnValue, sndCardOutput);
 * 
 * #ifndef WIN32
 * 	// The OSS component can use several encodings. We'll check
 * 	// what encoding type is being used and inform the sample encoder
 * 	// of this.
 * 	uint32_t audioSubtype = sndCardOutput.getRawAudioSubtype();
 * 	returnValue = sampEnc3.init(audioSubtype);
 * #else
 * 	// The WinMM soundcard output component uses 16 bit signed little
 * 	// endian data.
 * 	returnValue = sampEnc3.init(MIPRAWAUDIOMESSAGE_TYPE_S16LE);
 * #endif
 * 	checkError(returnValue, sampEnc3);
 * 
 * 	// Next, we'll create the chain
 * 	returnValue = chain.setChainStart(&timer);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&timer, &sndFileInput);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&sndFileInput, &sampConv);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&sampConv, &sampEnc);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&sampEnc, &uLawEnc);
 * 	checkError(returnValue, chain);
 * 	
 * 	returnValue = chain.addConnection(&uLawEnc, &rtpEnc);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&rtpEnc, &rtpComp);
 * 	checkError(returnValue, chain);
 * 	
 * 	returnValue = chain.addConnection(&rtpComp, &rtpDec);
 * 	checkError(returnValue, chain);
 * 
 * 	// This is where the feedback chain is specified: we want
 * 	// feedback from the mixer to reach the RTP audio decoder,
 * 	// so we'll specify that over the links in between, feedback
 * 	// should be transferred.
 * 
 * 	returnValue = chain.addConnection(&rtpDec, &uLawDec, true);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&uLawDec, &sampEnc2, true);
 * 	checkError(returnValue, chain);
 * 	
 * 	returnValue = chain.addConnection(&sampEnc2, &sampConv2, true);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&sampConv2, &mixer, true);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&mixer, &sampEnc3);
 * 	checkError(returnValue, chain);
 * 
 * 	returnValue = chain.addConnection(&sampEnc3, &sndCardOutput);
 * 	checkError(returnValue, chain);
 * 	
 * 	// Start the chain
 * 
 * 	returnValue = chain.start();
 * 	checkError(returnValue, chain);
 * 
 * 	// We'll wait until enter is pressed
 * 
 * 	getc(stdin);
 * 
 * 	returnValue = chain.stop();
 * 	checkError(returnValue, chain);
 * 
 * 	rtpSession.Destroy();
 * 	
 * 	// We'll let the destructors of the other components take care
 * 	// of their de-initialization.
 * 
 * #ifdef WIN32
 * 	WSACleanup();
 * #endif
 * 	return 0;
 * }
 * \endcode 
 *
 * 		If this example works, you should here the sound file 'soundfile.wav' being
 * 		played. Note that the file may sound different than expected since it is
 * 		resampled to a 8000Hz sampling rate.
 * 	
 * \section components Available components
 *
 * 	The easiest way to browse the components is to take a look at the directory structure.
 *
 * 	\warning In principle, a specific component instance can be used in multiple chains
 * 	         since each chain will lock the components being used. However, only few
 * 	         components are actually usable by multiple chains. For most components, synchronization
 * 	         problems will arise!
 * 
 * \section wrappers Available wrappers
 *
 * 	Currently, two wrapper classes are available:
 * 	 - MIPAudioSession:
 * 	   This is a wrapper for a simple voice over IP session. It uses a the speex codec for
 * 	   audio compression.
 * 	 - MIPVideoSession:
 * 	   This is a wrapper for s simple video over IP session. It uses libavcodec's H.263+
 * 	   codec.
 * 
 */

