class Signalling {
    /**
     * Interface to WebRTC demo signalling server.
     * Protocol: https://github.com/GStreamer/gstreamer/blob/main/subprojects/gst-examples/webrtc/signalling/Protocol.md
     *
     * @constructor
     * @param {URL} [server]
     *    The URL object of the signalling server to connect to, created with `new URL()`.
     *    Signalling implementation is here:
     *      https://github.com/GStreamer/gstreamer/tree/main/subprojects/gst-examples/webrtc/signalling
     * @param {Debug} [debug]
     */
    constructor(server, debug) {
        /**
         * @type {URL}
         */
        this._server = server;
        /**
         * @type {Debug}
         */
        this.debug = debug;
        /**
         * @event
         * @type {function}
         */
        this.onice = null;
        /**
         * @event
         * @type {function}
         */
        this.onsdp = null;
        /**
         * @event
         * @type {function}
         */
        this.ondisconnect = null;
        /**
         * @type {string}
         */
        this.state = 'disconnected';
        /**
         * @type {number}
         */
        this.retry_count = 0;
        /**
         * @type {function}
         */
        this.onconnected = null;
        /**
         * @type {array}
         */
        this.desktopList = []
        /**
         * @type {function}
         */
        this.onconnectError = null;
    }

    /**
     * Sets SDP
     *
     * @private
     * @param {String} message
     */
    _setSDP(sdp) {
        if (this.onsdp !== null) {
            this.onsdp(sdp);
        }
    }

    /**
     * Sets ICE
     *
     * @private
     * @param {RTCIceCandidate} icecandidate
     */
    _setICE(icecandidate) {
        if (this.onice !== null) {
            this.onice(icecandidate);
        }
    }
    /**
     * Fired whenever the signalling websocket is opened.
     * Sends the peer id to the signalling server.
     *
     * @private
     * @event
     */
    _onServerOpen() {
        // Send local device resolution and scaling with HELLO message.
        // var currRes = webrtc.input.getWindowResolution();
        // var meta = {
        //     "res": parseInt(currRes[0]) + "x" + parseInt(currRes[1]),
        //     "scale": window.devicePixelRatio
        // };

        // this._ws_conn.send(JSON.stringify({type: "HELLO", data: 0 }));

        // this._ws_conn.send(`HELLO ${this.peer_id} ${btoa(JSON.stringify(meta))}`);
        // this._setStatus("Registering with server, peer ID: " + this.peer_id);
        this.retry_count = 0;
    }
    /**
     * Fired whenever the signalling websocket emits and error.
     * Reconnects after 3 seconds.
     *
     * @private
     * @event
     */
    _onServerError() {
        this.debug.warning("Signalling", "Connection error, retry in 3 seconds.");
        // this._setStatus("Connection error, retry in 3 seconds.");
        this.retry_count++;
        if (this._ws_conn.readyState === this._ws_conn.CLOSED) {
            setTimeout(() => {
                if (this.retry_count > 3) {
                    this.debug.error("Signalling", "Connection error.");
                    if(this.onconnectError)
                        this.onconnectError()
                    // window.location.replace(window.location.href.replace(window.location.pathname, "/"));
                } else {
                    this.connect();
                }
            }, 3000);
        }
    }

    /**
     * Fired whenever a message is received from the signalling server.
     * Message types:
     *   HELLO: response from server indicating peer is registered.
     *   ERROR*: error messages from server.
     *   {"sdp": ...}: JSON SDP message
     *   {"ice": ...}: JSON ICE message
     *
     * @private
     * @event
     * @param {Event} event The event: https://developer.mozilla.org/en-US/docs/Web/API/MessageEvent
     */
    _onServerMessage(event) {
        var msg;
        try {
            msg = JSON.parse(event.data);
        } catch (e) {
            if (e instanceof SyntaxError) {
                this.debug.error("Signalling", "error parsing message as JSON.\n " + event.data);
            } else {
                this.debug.error("Signalling", "failed to parse message.\n " + event.data)
            }
            return;
        }
        switch(msg.type) {
            case "HELLO":
                this.desktopList = msg.data
                this.state = 'connected';
                this.debug.success("Signalling", "Connected to server.");
                if(this.onconnected !== null) {
                    this.onconnected();
                }
                break;
            case "sdp":
                let sdp = new RTCSessionDescription(msg.data);
                this._setSDP(sdp);
                break;
            case "ice":
                let icecandidate = new RTCIceCandidate(msg.data);
                this._setICE(icecandidate);
                break
            default:
                this.debug.error("Signalling", "unhandled JSON message.\n" + msg);
        }
    }
    /**
     * Fired whenever the signalling websocket is closed.
     * Reconnects after 1 second.
     *
     * @private
     * @event
     */
    _onServerClose() {
        if (this.state !== 'connecting') {
            this.state = 'disconnected';
            this.debug.warning("Signalling", "Server closed connection.");
            if (this.ondisconnect !== null) this.ondisconnect();
        }
    }

    sendConnectMsg(mediaConfig) {
        if(mediaConfig === null) {
            mediaConfig = { desktopID:0, video:true, audio:true };
        }
        // let config = {
        //     "desktopID": mediaConfig.desktopID,
        //     "video": mediaConfig.video,
        //     "audio": mediaConfig.audio
        // };
        this._ws_conn.send(JSON.stringify({type: "connect", data: mediaConfig }));
    }
    /**
     * Initiates the connection to the signalling server.
     * After this is called, a series of handshakes occurs between the signalling
     * server and the server (peer) to negotiate ICE candiates and media capabilities.
     */
    connect() {
        // this.mediaConfig = mediaConfig;
        // if(this.mediaConfig === null) {
        //     this.mediaConfig = { video:true, audio:true };
        // }
        this.state = 'connecting';
        this.debug.print("Signalling", "Connecting to server...");

        this._ws_conn = new WebSocket(this._server);
        // Bind event handlers.
        this._ws_conn.addEventListener('open', this._onServerOpen.bind(this));
        this._ws_conn.addEventListener('error', this._onServerError.bind(this));
        this._ws_conn.addEventListener('message', this._onServerMessage.bind(this));
        this._ws_conn.addEventListener('close', this._onServerClose.bind(this));
    }

    /**
     * Closes connection to signalling server.
     * Triggers onServerClose event.
     */
    disconnect() {
        this._ws_conn.close();
    }

    /**
     * Send ICE candidate.
     *
     * @param {RTCIceCandidate} ice
     */
    sendICE(ice) {
        this.debug.print("Signalling", "sending ice: " + JSON.stringify(ice));
        // this._setDebug("sending ice candidate: " + JSON.stringify(ice));
        this._ws_conn.send(JSON.stringify({ id: 0, type: "ice", data: ice }));
    }

    /**
     * Send local session description.
     *
     * @param {RTCSessionDescription} sdp
     */
    sendSDP(sdp) {
        this.debug.print("Signalling", "sending local " + sdp.type + ".\n" + sdp.sdp);
        // this._setDebug("sending local sdp: " + JSON.stringify(sdp));
        this._ws_conn.send(JSON.stringify({ id: 0, type: "sdp", data: sdp }));
    }
}