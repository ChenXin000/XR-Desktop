
var deskIndex = 0;
var desktopList = []
let isAvSync = false; // 音视频同步
let isOpenOperate = false; // 是否开启远程操作
let checkWaitTimer = null; // 操作码验证超时计时器

let videoBytesReceivedStart = 0;
let audioBytesReceivedStart = 0;

let connectionPacketsReceived = 0;
let connectionPacketsLost = 0;

let connectionStatType = '';
let connectionBytesReceived = 0;
let connectionBytesSent = 0;
let connectionAvailableBandwidth = 0;

let connectionAudioLatency = 0;
let connectionAudioCodecName = '';
let connectionAudioBitrate = 0;

let connectionVideoLatency = 0;
let connectionCodec = '';
let connectionVideoDecoder = '';
let connectionResolution = '';
let connectionFrameRate = 0;
let connectionVideoBitrate = 0;

let port = window.location.port ? parseInt(window.location.port) : 80
let server = "ws://" + window.location.hostname + ":" + (port + 1) + "/" + "ws";
let rtcPeerConfig = {
    // "lifetimeDuration": "86400s",
    "iceServers": [
        {
            "urls": [
                "stun:stun.l.google.com:19302"
            ]
        },
    ],
    // "blockStatus": "NOT_BLOCKED",
    "iceTransportPolicy": "all",
};
let debug = new Debug(debug_text);
let videosignaling = new Signalling(server, debug);
let audioSignaling = new Signalling(server, debug);
let videoWebrtc = new Webrtc(videosignaling, videoElement, rtcPeerConfig, debug);
let audioWebrtc = new Webrtc(audioSignaling, audioElement, rtcPeerConfig, debug);
let input_ = new Input(videoElement,(data)=>{ videoWebrtc._send_channel.send('i,' + data); })


videosignaling.onconnected = () => {
    if(videosignaling.state === "connected" && audioSignaling.state === "connected") {
        desktopList = videosignaling.desktopList;
        if(desktopList.length) {
            document.getElementById("desk-id-text").innerText = "桌面：" + (desktopList[0] + 1)
            showMainContainer(true);
        } else {
            alert("请至少开启一个桌面后，刷新此页面重试。");
        }

    }
}
audioSignaling.onconnected =() => {
    if(videosignaling.state === "connected" && audioSignaling.state === "connected") {
        desktopList = videosignaling.desktopList;
        if(desktopList.length) {
            document.getElementById("desk-id-text").innerText = "桌面：" + (desktopList[0] + 1)
            showMainContainer(true);
        } else {
            alert("请至少开启一个桌面后，刷新此页面重试。");
        }
    }
}

videosignaling.onconnectError = () => {
    alert("连接失败")
}

audioWebrtc.onconnectionstatechange = (state) => {
    if(state === 'connected' && videoWebrtc._connected) {
        getConnectionStats();
    }
}

videoWebrtc.onconnectionstatechange = (state) => {
    switch (state) {
        case "connected":
            if(isAvSync || audioWebrtc._connected) {
                getConnectionStats();
            }
            break;

        case "disconnected":
            loading_container.style.display = ""
            // debugElement.style.display = ""
            closeFullscreen(videoElement);
            break;

        case "failed":

            break;
        default:
    }
}
videoWebrtc.onplaystreamrequired = (state) => {
    loading_container.style.display = "none";
}
// 监听操作码状态变化
videoWebrtc.onopcodechange = (state) => {
    clearTimeout(checkWaitTimer);
    if(!state) {
        input_.opcode_ = '';
        alert("操作码错误！");
        optInputBox.disabled = false;
        showInputBox(true);
    }
    openRemoting();
}
audioWebrtc.onplaystreamrequired = (state) => {
    loading_container.style.display = "none";
    // if(state) {
    // let audBut = document.getElementById("audio-but");

    //     audBut.style.backgroundColor = "#2189ff";
    //     audBut.style.backgroundImage = "url('./image/audio-on.svg')";
    // }
}
// 获取已连接webrtc的统计信息
function getConnectionStats() {
    let statsStart = new Date().getTime() / 1000;
    // Sum of video+audio packets.
    let statsLoop = () => {
        videoWebrtc.getConnectionStats().then(async (stats) => {
            let now = new Date().getTime() / 1000;
            // Sum of video+audio+server latency in ms.
            let connectionLatency = 0;
            // app.connectionLatency += app.serverLatency;
    
            // Connection stats
            connectionStatType = stats.general.connectionType;
            connectionBytesReceived = (stats.general.bytesReceived * 1e-6).toFixed(2) + " MBytes";
            connectionBytesSent = (stats.general.bytesSent * 1e-6).toFixed(2) + " MBytes";
            connectionAvailableBandwidth = (parseInt(stats.general.availableReceiveBandwidth) / 1e+6).toFixed(2) + " mbps";
            // Video stats.
            connectionVideoLatency = parseInt(stats.video.jitterBufferDelay * 1000);
            connectionLatency += stats.video.jitterBufferDelay * 1000;
            connectionPacketsReceived += stats.video.packetsReceived;
            connectionPacketsLost += stats.video.packetsLost;
            connectionCodec = stats.video.codecName;
            connectionVideoDecoder = stats.video.decoder;
            connectionResolution = stats.video.frameWidth + "x" + stats.video.frameHeight;
            connectionFrameRate = stats.video.framesPerSecond;
            connectionVideoBitrate = (((stats.video.bytesReceived - videoBytesReceivedStart) / (now - statsStart)) * 8 / 1e+6).toFixed(2);
            videoBytesReceivedStart = stats.video.bytesReceived;
            // Audio stats.
            if(isAvSync) {
                connectionLatency += stats.audio.jitterBufferDelay * 1000;
                connectionPacketsReceived += stats.audio.packetsReceived;
                connectionPacketsLost += stats.audio.packetsLost;
                connectionAudioLatency = parseInt(stats.audio.jitterBufferDelay * 1000);
                connectionAudioCodecName = stats.audio.codecName;
                connectionAudioBitrate = (((stats.audio.bytesReceived - audioBytesReceivedStart) / (now - statsStart)) * 8 / 1e+3).toFixed(2);
                audioBytesReceivedStart = stats.audio.bytesReceived;
            } else {
                await audioWebrtc.getConnectionStats().then((stats) => {
                    connectionLatency += stats.audio.jitterBufferDelay * 1000;
                    connectionPacketsReceived += stats.audio.packetsReceived;
                    connectionPacketsLost += stats.audio.packetsLost;
                    connectionAudioLatency = parseInt(stats.audio.jitterBufferDelay * 1000);
                    connectionAudioCodecName = stats.audio.codecName;
                    connectionAudioBitrate = (((stats.audio.bytesReceived - audioBytesReceivedStart) / (now - statsStart)) * 8 / 1e+3).toFixed(2);
                    audioBytesReceivedStart = stats.audio.bytesReceived;
                });
            }

            statsStart = now;
            showStats(videoWebrtc.peerConnection.connectionState);
            // Stats refresh loop.
            setTimeout(statsLoop, 1000);
        });
    };
    statsLoop();
}

// 验证操作是否正确
function checkOpcode() {
    let value = optInputBox.value;
    if(value.length === 6) {
        optInputBox.disabled = true;
        input_.checkOpcode(value);
        checkWaitTimer = setTimeout(()=>{
            alert('请求超时！');
            input_.opcode_ = '';
            showInputBox(true);
        },2000)
    }
}
// 开启远程操作
function openRemoting() {
    if(isOpenOperate) {
        switchRemotingBut(false);
        input_.detach();
        isOpenOperate = false;
    } else if(input_.opcode_) {
        switchRemotingBut(true);
        input_.attach();
        isOpenOperate = true;
    } else {
    console.log(123)

        showInputBox(true);
    }
}

// 桌面ID加一
function desktopIDAdd() {
    let deskIDEle = document.getElementById("desk-id-text");
    if(deskIndex + 1 < desktopList.length) {
        deskIndex++;
        deskIDEle.innerText = "桌面：" + (desktopList[deskIndex] + 1)
    }
}
// 桌面ID减一
function desktopIDSub() {
    let deskIDEle = document.getElementById("desk-id-text");
    if(deskIndex > 0){
        deskIndex--;
        deskIDEle.innerText = "桌面：" + (desktopList[deskIndex] + 1)
    } 
}
function openAudioSync() {
    if(isAvSync) isAvSync = false;
    else isAvSync = true;
}

// 开始连接webrtc
function start() {
    if(isAvSync) {
        videoWebrtc.connect({"desktopID":desktopList[deskIndex],"video":true,"audio":true});
    } else {
        videoWebrtc.connect({"desktopID":desktopList[deskIndex],"video":true,"audio":false});
        audioWebrtc.connect({"desktopID":desktopList[deskIndex],"video":false,"audio":true});
    }
}

window.addEventListener("load",() => {
    videosignaling.connect();
    audioSignaling.connect();
})


  