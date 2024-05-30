class Webrtc {
    /**
     * Interface to WebRTC.
     *
     * @constructor
     * @param {Signalling} [signalling]
     *    Instance of WebRTCDemoSignalling used to communicate with signalling server.
     * @param {Element} [mediaElement]
     *    video element to attach stream to.
     * @param {Object} [rtcPeerConfig]
     * @param {Debug} [debug]
     */
    constructor(signalling, mediaElement, rtcPeerConfig, debug) {
        /**
         * @type {Signalling}
         */
        this.signalling = signalling;
        /**
         * @type {Element}
         */
        this.mediaElement = mediaElement;
        // /**
        //  * @type {Element}
        //  */
        // this.audioElement = audioElement;
        /**
         * @type {Object}
         */
        this.rtcPeerConfig = rtcPeerConfig;
        /**
         * @type {Debug}
         */
        this.debug = debug;
        /**
         * @type {RTCPeerConnection}
         */
        this.peerConnection = null;
        /**
         * @type {boolean}
         */
        this._connected = false;
        /**
         * @type {function}
         */
        this.onplaystreamrequired = null;
        /**
         * @type {function}
         */
        this.onconnectionstatechange = null;
        /**
         * @type {RTCDataChannel}
         */
        this._send_channel = null;
        /**
         * @type {function}
         */
        this.ondatachannelopen = null;
        /**
         * @type {function}
         */
        this.ondatachannelclose = null;
        /**
         * @type {function}
         */
        this.onopcodechange = null;

        // Bind signalling server callbacks.
        this.signalling.onsdp = this._onSDP.bind(this);
        this.signalling.onice = this._onSignallingICE.bind(this);
    }
    /**
     * Handler for peer connection state change.
     * Possible values for state:
     *   connected
     *   disconnected
     *   failed
     *   closed
     * @param {String} state
     */
    _handleConnectionStateChange(state) {
        switch (state) {
            case "connected":
                this.debug.success("Webrtc", "Connection complete.");
                // this._setStatus("Connection complete.");
                this._connected = true;
                break;

            case "disconnected":
                this.debug.error("Webrtc", "Peer connection disconnected.");
                // this._setError("Peer connection disconnected");
                // if (this._send_channel !== null && this._send_channel.readyState === 'open') {
                //     this._send_channel.close();
                // }
                this.mediaElement.load();
                this._connected = false;
                break;

            case "failed":
                this.debug.error("Webrtc", "Peer connection failed.");
                // this._setError("Peer connection failed");
                this.mediaElement.load();
                this._connected = false;
                break;
            default:
        }
    }

    /**
     * Handler for ICE candidate received from peer connection.
     * If ice is null, then all candidates have been received.
     *
     * @event
     * @param {RTCPeerConnectionIceEvent} event - The event: https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnectionIceEvent
     */
    _onPeerICE(event) {
        if (event.candidate === null) {
            this.debug.success("Webrtc", "all ICE candidates have been received.");
            // this._setStatus("Completed ICE candidates from peer connection");
            return;
        }
        // this.debug.print("Webrtc", "sending ice candidate:\n" + JSON.stringify(event.candidate));
        this.signalling.sendICE(event.candidate);
    }

    /**
     * Handles incoming ICE candidate from signalling server.
     *
     * @param {RTCIceCandidate} icecandidate
     */
    _onSignallingICE(icecandidate) {
        this.debug.print("Webrtc", "received ice: " + JSON.stringify(icecandidate));
        // this._setDebug("received ice candidate from signalling server: " + JSON.stringify(icecandidate));
        // if (this.forceTurn && JSON.stringify(icecandidate).indexOf("relay") < 0) { // if no relay address is found, assuming it means no TURN server
        //     this._setDebug("Rejecting non-relay ICE candidate: " + JSON.stringify(icecandidate));
        //     return;
        // }
        // console.log(icecandidate);

        this.peerConnection.addIceCandidate(icecandidate).catch((error) => {
            this.debug.error("Webrtc", error);
        });
    }
    /**
     * creates an answer with a local description and sends that to the peer.
     */
    _createAnswer() {
        this.peerConnection.createAnswer()
            .then((local_sdp) => {
                // Override SDP to enable stereo on WebRTC Opus with Chromium, must be munged before the Local Description
                if (local_sdp.sdp.indexOf('multiopus') === -1) {
                    if (!(/[^-]stereo=1/gm.test(local_sdp.sdp))) {
                        this.debug.warning("Webrtc", "Overriding WebRTC SDP to allow stereo audio");
                        // console.log("Overriding WebRTC SDP to allow stereo audio");
                        if (/[^-]stereo=0/gm.test(local_sdp.sdp)) {
                            local_sdp.sdp = local_sdp.sdp.replace('stereo=0', 'stereo=1');
                        } else {
                            local_sdp.sdp = local_sdp.sdp.replace('useinbandfec=', 'stereo=1;useinbandfec=');
                        }
                    }
                }
                this.debug.print("Webrtc", "Created local " + local_sdp.type + ".\n" + local_sdp.sdp);
                // console.log("Created local SDP", local_sdp);
                this.peerConnection.setLocalDescription(local_sdp).then(() => {
                    // this._setDebug("Sending SDP answer");
                    // this.debug.print("Webrtc", "Sending SDP answer.");
                    this.signalling.sendSDP(this.peerConnection.localDescription);
                });
            }).catch(() => {
                this.debug.error("Webrtc", "Error creating local SDP.");
                // this._setError("Error creating local SDP");
            });
    
    }

    /**
     * Handles incoming SDP from signalling server.
     * Sets the remote description on the peer connection,
     *
     * @param {RTCSessionDescription} sdp
     */
    _onSDP(sdp) {
        if (sdp.type != "offer") {
            this.debug.error("Webrtc", "received SDP was not type offer.");
            // this._setError("received SDP was not type offer.");
            return
        }
        // console.log("Received remote SDP", sdp);
        this.debug.print("Webrtc", "Received remote " + sdp.type + ".\n" + sdp.sdp);
        this.peerConnection.setRemoteDescription(sdp).then(() => {
            this.debug.success("Webrtc", "received SDP offer, creating answer...");
            // this._setDebug("received SDP offer, creating answer");
            this._createAnswer();
        })
    }

    /**
     * Handles incoming track event from peer connection.
     *
     * @param {Event} event - Track event: https://developer.mozilla.org/en-US/docs/Web/API/RTCTrackEvent
     */
    _ontrack(event) {
        // this._setStatus("Received incoming " + event.track.kind + " stream from peer");
        this.debug.print("Webrtc", "Received incoming " + event.track.kind + " stream from peer.");
        // if (!this.streams) this.streams = [];
        // this.streams.push([event.track.kind, event.streams]);
        if (event.track.kind === "video" || event.track.kind === "audio") {
            this.mediaElement.srcObject = event.streams[0];
            this.playStream(event.track.kind);
        }
        // else if(event.track.kind === "audio") {
        //     this.audioElement.srcObject = event.streams[0];
        //     // this.playStream(event.track.kind, this.audioElement);
        // }
    }
    /**
     * Handles messages from the peer data channel.
     * @param {MessageEvent} event
     */
    _onPeerDataChannelMessage(event) {
        let msg;
        try {
            msg = JSON.parse(event.data);
        } catch (e) {
            if (e instanceof SyntaxError) {
                this.debug.error("error parsing data channel message as JSON: " + event.data);
            } else {
                this.debug.error("failed to parse data channel message: " + event.data);
            }
            return;
        }
        if(msg.type === "ping") {
            this.sendDataChannelMessage("pong");
        }
        if(msg.type === 'o') {
            if(this.onopcodechange) {
                this.onopcodechange(msg.data);
            }
        }
    
    }
    /**
     * Handles incoming data channel events from the peer connection.
     *
     * @param {RTCdataChannelEvent} event
     */
    _onPeerdDataChannel(event) {
        console.log("Peer data channel created: " + event.channel.label);
        // event.channel.send("123456")
        // this._setStatus("Peer data channel created: " + event.channel.label);

        // Bind the data channel event handlers.
        this._send_channel = event.channel;
        this._send_channel.onmessage = this._onPeerDataChannelMessage.bind(this);
        this._send_channel.onopen = () => {
            // this._send_channel.send("HELLO");

            if (this.ondatachannelopen !== null)
                this.ondatachannelopen();
        }
        this._send_channel.onclose = () => {
            if (this.ondatachannelclose !== null)
                this.ondatachannelclose();
        }
    }

    sendDataChannelMessage(msg) {
        this._send_channel.send(msg);
    }

    /**
     * Gets connection stats. returns new promise.
     */
    getConnectionStats() {
        var pc = this.peerConnection;
        var connectionDetails = {
            // General connection stats
            general: {
                bytesReceived: 0, // from the transport
                bytesSent: 0, // from the transport
                connectionType: "NA", // from the transport.candiate-pair.remote-candidate
                availableReceiveBandwidth: 0, // from transport.candidate-pair
            },

            // Video stats
            video: {
                bytesReceived: 0, //from incoming-rtp
                decoder: "NA", // from incoming-rtp
                frameHeight: 0, // from incoming-rtp
                frameWidth: 0, // from incoming-rtp
                framesPerSecond: 0, // from incoming-rtp
                packetsReceived: 0, // from incoming-rtp
                packetsLost: 0, // from incoming-rtp
                codecName: "NA", // from incoming-rtp.codec
                jitterBufferDelay: 0, // from track.jitterBufferDelay / track.jitterBuffferEmittedCount in seconds.
            },

            // Audio stats
            audio: {
                bytesReceived: 0, // from incomine-rtp
                packetsReceived: 0, // from incoming-rtp
                packetsLost: 0, // from incoming-rtp
                codecName: "NA", // from incoming-rtp.codec
                jitterBufferDelay: 0, // from track.jitterBufferDelay / track.jitterBuffferEmittedCount in seconds.
            }
        };

        return new Promise(function (resolve, reject) {
            // Statistics API:
            // https://developer.mozilla.org/en-US/docs/Web/API/WebRTC_Statistics_API
            pc.getStats(null).then( (stats) => {
                var reports = {
                    transports: {},
                    candidatePairs: {},
                    remoteCandidates: {},
                    codecs: {},
                    videoRTP: null,
                    audioRTP: null,
                }

                var allReports = [];

                stats.forEach( (report) => {
                    allReports.push(report);
                    if (report.type === "transport") {
                        reports.transports[report.id] = report;
                    } else if (report.type === "candidate-pair") {
                        reports.candidatePairs[report.id] = report;
                    } else if (report.type === "inbound-rtp") {
                        // Audio or video stat
                        // https://w3c.github.io/webrtc-stats/#streamstats-dict*
                        if (report.kind === "video") {
                            reports.videoRTP = report;
                        } else if (report.kind === "audio") {
                            reports.audioRTP = report;
                        }
                    } else if (report.type === "remote-candidate") {
                        reports.remoteCandidates[report.id] = report;
                    } else if (report.type === "codec") {
                        reports.codecs[report.id] = report;
                    }
                });

                // Extract video related stats.
                var videoRTP = reports.videoRTP;
                if (videoRTP !== null) {
                    connectionDetails.video.bytesReceived = videoRTP.bytesReceived;
                    connectionDetails.video.decoder = videoRTP.decoderImplementation;
                    connectionDetails.video.frameHeight = videoRTP.frameHeight;
                    connectionDetails.video.frameWidth = videoRTP.frameWidth;
                    connectionDetails.video.framesPerSecond = videoRTP.framesPerSecond;
                    connectionDetails.video.packetsReceived = videoRTP.packetsReceived;
                    connectionDetails.video.packetsLost = videoRTP.packetsLost;
                    connectionDetails.video.jitterBufferDelay = reports.videoRTP.jitterBufferDelay / reports.videoRTP.jitterBufferEmittedCount;

                    // Extract video codec from found codecs.
                    var codec = reports.codecs[videoRTP.codecId];
                    if (codec !== undefined) {
                        connectionDetails.video.codecName = codec.mimeType.split("/")[1];
                    }
                }

                // Extract audio related stats.
                var audioRTP = reports.audioRTP;
                if (audioRTP !== null) {
                    connectionDetails.audio.bytesReceived = audioRTP.bytesReceived;
                    connectionDetails.audio.packetsReceived = audioRTP.packetsReceived;
                    connectionDetails.audio.packetsLost = audioRTP.packetsLost;
                    connectionDetails.audio.jitterBufferDelay = reports.audioRTP.jitterBufferDelay / reports.audioRTP.jitterBufferEmittedCount;

                    // Extract audio codec from found codecs.
                    var codec = reports.codecs[audioRTP.codecId];
                    if (codec !== undefined) {
                        connectionDetails.audio.codecName = codec.mimeType.split("/")[1];
                    }
                }

                // Extract transport stats.
                if (Object.keys(reports.transports).length > 0) {
                    var transport = reports.transports[Object.keys(reports.transports)[0]];
                    connectionDetails.general.bytesReceived = transport.bytesReceived;
                    connectionDetails.general.bytesSent = transport.bytesSent;

                    // Get the connection-pair
                    var candidatePair = reports.candidatePairs[transport.selectedCandidatePairId];
                    if (candidatePair !== undefined) {
                        connectionDetails.general.availableReceiveBandwidth = candidatePair.availableIncomingBitrate;
                        var remoteCandidate = reports.remoteCandidates[candidatePair.remoteCandidateId]
                        if (remoteCandidate !== undefined) {
                            connectionDetails.general.connectionType = remoteCandidate.candidateType;
                        }
                    }
                }

                // Compute total packets received and lost
                connectionDetails.general.packetsReceived = connectionDetails.video.packetsReceived + connectionDetails.audio.packetsReceived;
                connectionDetails.general.packetsLost = connectionDetails.video.packetsLost + connectionDetails.audio.packetsLost;

                // DEBUG
                connectionDetails.reports = reports;
                connectionDetails.allReports = allReports;

                resolve(connectionDetails);
            }).catch( (e) => reject(e));
        });
    }

    /**
     * Starts playing the stream.
     * Note that this must be called after some DOM interaction has already occured.
     * Chrome does not allow auto playing of videos without first having a DOM interaction.
     */
    playStream(kind) {
        this.mediaElement.load();

        let playPromise = this.mediaElement.play();
        if (playPromise !== undefined) {
            playPromise.then(() => {
                if (this.onplaystreamrequired !== null) {
                    this.onplaystreamrequired(true);
                }
                this.debug.success("Webrtc", kind + " Stream is playing.");
                // this._setDebug("Stream is playing.");
            }).catch(() => {
                if (this.onplaystreamrequired !== null) {
                    this.onplaystreamrequired(false);
                }
                this.debug.error("Webrtc", kind + " Stream play failed.");
                // this._setDebug("Stream play failed and no onplaystreamrequired was bound.");
            });
        }
    }
    /**
     * Initiate connection to signalling server.
     */
    connect(mediaConfig) {
        // Create the peer connection object and bind callbacks.
        this.peerConnection = new RTCPeerConnection(this.rtcPeerConfig);
        this.peerConnection.ontrack = this._ontrack.bind(this);
        this.peerConnection.onicecandidate = this._onPeerICE.bind(this);
        // 仅视频流创建数据通道和输入事件
        // if(mediaConfig.video) {
            this.peerConnection.ondatachannel = this._onPeerdDataChannel.bind(this);
            // this.input = Input(this.mediaElement,(data)=> {
            //     this._send_channel.send('i,' + data);
            // })
        // }

        this.peerConnection.onconnectionstatechange = () => {
            // Pass state to event listeners.
            if(this.onconnectionstatechange !== null) {
                this.onconnectionstatechange(this.peerConnection.connectionState);
            }
            
            // Local event handling.
            this._handleConnectionStateChange(this.peerConnection.connectionState);


            // this._setConnectionState(this.peerConnection.connectionState);
        };

        // if (this.forceTurn) {
        //     this._setStatus("forcing use of TURN server");
        //     var config = this.peerConnection.getConfiguration();
        //     config.iceTransportPolicy = "relay";
        //     this.peerConnection.setConfiguration(config);
        // }

        // this.signalling.peer_id = this.peer_id;
        this.signalling.sendConnectMsg(mediaConfig);
    }

    close() {
        if (this.peerConnection !== null)  {
            this.peerConnection.close();
        }
    }
    /**
     * Attempts to reset the webrtc connection by:
     *   1. Closing the data channel gracefully.
     *   2. Closing the RTC Peer Connection gracefully.
     *   3. Reconnecting to the signaling server.
     */
    reset() {
        // Clear cursor cache.
        // this.cursor_cache = new Map();

        let signalState = this.peerConnection.signalingState;
        // if (this._send_channel !== null && this._send_channel.readyState === "open") {
        //     this._send_channel.close();
        // }
        if (this.peerConnection !== null) this.peerConnection.close();
        if (signalState !== "stable") {
            setTimeout(() => {
                this.connect();
            }, 3000);
        } else {
            this.connect();
        }
    }
}