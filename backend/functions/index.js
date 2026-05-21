const { onSchedule } = require("firebase-functions/v2/scheduler");
const { onValueWritten } = require("firebase-functions/v2/database");
const { onDocumentWritten } = require("firebase-functions/v2/firestore");
const { initializeApp } = require("firebase-admin/app");
const { getFirestore, FieldValue, Timestamp } = require("firebase-admin/firestore");
const { getDatabase } = require("firebase-admin/database");
const { getMessaging } = require("firebase-admin/messaging");

initializeApp();

const db = getFirestore();
const rtdb = getDatabase();

// ── 1. Promote pending notifications from RTDB → Firestore ───────
// Fires whenever ESP32 writes a notification to RTDB
exports.promotePendingNotification = onValueWritten(
  "devices/{deviceId}/pendingNotification",
  async (event) => {
    const deviceId = event.params.deviceId;
    const notif = event.data.after.val();

    if (!notif || !notif.type) return;

    try {
      // Find users who own this device
      const usersSnap = await db
        .collection("users")
        .where("deviceIds", "array-contains", deviceId)
        .get();

      if (usersSnap.empty) {
        console.log(`No users found for device ${deviceId}`);
        return;
      }

      const batch = db.batch();
      const fcmTokens = [];

      for (const userDoc of usersSnap.docs) {
        const userId = userDoc.id;
        const fcmToken = userDoc.data().fcmToken;
        if (fcmToken) fcmTokens.push(fcmToken);

        // Save to Firestore notifications collection
        const notifRef = db.collection("notifications").doc();
        batch.set(notifRef, {
          id: notifRef.id,
          deviceId,
          userId,
          type: notif.type,
          title: notif.title,
          body: notif.body,
          severity: getSeverity(notif.type),
          isRead: false,
          data: {},
          timestamp: FieldValue.serverTimestamp(),
        });
      }

      await batch.commit();

      // Send FCM push notifications
      if (fcmTokens.length > 0) {
        const messaging = getMessaging();
        for (const token of fcmTokens) {
          try {
            await messaging.send({
              token,
              notification: {
                title: notif.title,
                body: notif.body,
              },
              android: {
                notification: {
                  channelId: "pet_feeder_alerts",
                  priority: "high",
                },
              },
              apns: {
                payload: {
                  aps: { sound: "default" },
                },
              },
            });
          } catch (fcmErr) {
            console.error("FCM error for token:", fcmErr.message);
          }
        }
      }

      // Clear the pending notification from RTDB
      await rtdb
        .ref(`devices/${deviceId}/pendingNotification`)
        .remove();

      console.log(`Notification promoted for device ${deviceId}: ${notif.title}`);
    } catch (err) {
      console.error("promotePendingNotification error:", err);
    }
  }
);

// ── 2. Mark device offline if heartbeat is stale (runs every 5 min) ─
exports.checkDeviceHeartbeat = onSchedule("every 5 minutes", async () => {
  const devicesSnap = await rtdb.ref("devices").once("value");
  const devices = devicesSnap.val();
  if (!devices) return;

  const STALE_THRESHOLD_MS = 5 * 60 * 1000; // 5 minutes
  const now = Date.now();

  const updates = {};
  for (const [deviceId, deviceData] of Object.entries(devices)) {
    const lastSeen = (deviceData.status?.lastSeen ?? 0) * 1000;
    const isOnline = deviceData.status?.online ?? false;

    if (isOnline && now - lastSeen > STALE_THRESHOLD_MS) {
      console.log(`Marking device ${deviceId} offline (stale heartbeat)`);
      updates[`devices/${deviceId}/status/online`] = false;

      // Notify users
      const usersSnap = await db
        .collection("users")
        .where("deviceIds", "array-contains", deviceId)
        .get();

      const batch = db.batch();
      for (const userDoc of usersSnap.docs) {
        const ref = db.collection("notifications").doc();
        batch.set(ref, {
          deviceId,
          userId: userDoc.id,
          type: "device_offline",
          title: "Feeder Offline",
          body: "Your smart feeder has gone offline. Check your WiFi connection.",
          severity: "warning",
          isRead: false,
          data: {},
          timestamp: FieldValue.serverTimestamp(),
        });
      }
      await batch.commit();
    }
  }

  if (Object.keys(updates).length > 0) {
    await rtdb.ref().update(updates);
  }
});

// ── 3. Clean up old data (runs daily) ─────────────────────────────
exports.dailyCleanup = onSchedule("every 24 hours", async () => {
  const now = new Date();

  const collections = [
    { name: "sensorSnapshots", days: 30 },
    { name: "feedingLogs",     days: 90 },
    { name: "waterLogs",       days: 90 },
    { name: "notifications",   days: 30 },
    { name: "aiInsights",      days: 60 },
  ];

  for (const col of collections) {
    const cutoff = new Date(now.getTime() - col.days * 24 * 60 * 60 * 1000);
    const cutoffTimestamp = Timestamp.fromDate(cutoff);

    const snap = await db
      .collection(col.name)
      .where("timestamp", "<", cutoffTimestamp)
      .limit(500)
      .get();

    if (!snap.empty) {
      const batch = db.batch();
      snap.docs.forEach((doc) => batch.delete(doc.ref));
      await batch.commit();
      console.log(`Deleted ${snap.size} old ${col.name} documents`);
    }
  }
});

// ── 4. Sync Firestore schedules → RTDB settings_cache ─────────────
// Fires whenever any schedule document is created, updated, or deleted.
// Reads ALL schedules for that device and writes them atomically into
// RTDB so the ESP32 can pick them up on the next settings_cache poll.
exports.syncSchedulesToRTDB = onDocumentWritten(
  "schedules/{scheduleId}",
  async (event) => {
    // Determine the deviceId from either the before or after snapshot
    const afterData  = event.data.after.exists  ? event.data.after.data()  : null;
    const beforeData = event.data.before.exists ? event.data.before.data() : null;
    const deviceId   = (afterData ?? beforeData)?.deviceId;

    if (!deviceId) {
      console.warn("syncSchedulesToRTDB: schedule has no deviceId, skipping.");
      return;
    }

    try {
      // Fetch all schedules for this device from Firestore
      const snap = await db
        .collection("schedules")
        .where("deviceId", "==", deviceId)
        .get();

      // Build a compact array that the ESP32 firmware expects:
      //   { id, hour, minute, portionGrams, enabled, days[] }
      const schedules = snap.docs.map((doc) => {
        const d = doc.data();
        return {
          id:           doc.id,
          hour:         d.hour         ?? 8,
          minute:       d.minute       ?? 0,
          portionGrams: d.portionGrams ?? 80,
          enabled:      d.enabled      ?? true,
          days:         d.days         ?? [0, 1, 2, 3, 4, 5, 6], // 0=Sun … 6=Sat
          label:        d.label        ?? "",
        };
      });

      // Write the full schedule list atomically into RTDB
      await rtdb
        .ref(`devices/${deviceId}/settings_cache/schedules`)
        .set(schedules);

      // Also stamp an updatedAt so the ESP32 can detect a change cheaply
      await rtdb
        .ref(`devices/${deviceId}/settings_cache/schedulesUpdatedAt`)
        .set(Math.floor(Date.now() / 1000));

      console.log(
        `syncSchedulesToRTDB: wrote ${schedules.length} schedule(s) ` +
        `for device ${deviceId}`
      );
    } catch (err) {
      console.error("syncSchedulesToRTDB error:", err);
    }
  }
);

// ── Helper ─────────────────────────────────────────────────────────
function getSeverity(type) {
  const critical = ["reservoir_empty", "device_offline", "emergency_stop"];
  const warning  = ["food_empty", "food_low", "water_low", "feeding_failed",
                    "jam_detected", "overfeeding_risk"];
  if (critical.includes(type)) return "critical";
  if (warning.includes(type)) return "warning";
  return "info";
}
