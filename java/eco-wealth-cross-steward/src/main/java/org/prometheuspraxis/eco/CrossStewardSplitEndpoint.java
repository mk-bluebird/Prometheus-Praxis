package org.prometheuspraxis.eco;

import javax.websocket.OnClose;
import javax.websocket.OnError;
import javax.websocket.OnMessage;
import javax.websocket.OnOpen;
import javax.websocket.Session;
import javax.websocket.server.ServerEndpoint;
import java.io.IOException;
import java.util.Set;
import java.util.concurrent.CopyOnWriteArraySet;

/**
 * WebSocket endpoint broadcasting cross-steward split attestations
 * to connected clients (e.g., mobile notification service).
 */
@ServerEndpoint("/ws/eco-wealth/splits")
public class CrossStewardSplitEndpoint {

    private static final Set<Session> SESSIONS = new CopyOnWriteArraySet<>();

    @OnOpen
    public void onOpen(Session session) {
        SESSIONS.add(session);
    }

    @OnClose
    public void onClose(Session session) {
        SESSIONS.remove(session);
    }

    @OnError
    public void onError(Session session, Throwable throwable) {
        // Log and remove session if needed.
        SESSIONS.remove(session);
    }

    @OnMessage
    public void onMessage(String message, Session session) {
        // This endpoint primarily broadcasts server-originated messages.
        // Optionally handle ping/pong or client control messages here.
    }

    /**
     * Broadcast a cross-steward split attestation to all connected clients.
     * This is called from the Rust bridge via JNI.
     */
    public static void broadcastAttestation(String jsonAttestation) {
        for (Session session : SESSIONS) {
            try {
                session.getBasicRemote().sendText(jsonAttestation);
            } catch (IOException e) {
                // On failure, drop the session.
                SESSIONS.remove(session);
            }
        }
    }
}
