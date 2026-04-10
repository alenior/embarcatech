import {onValueCreated} from "firebase-functions/v2/database";
import {initializeApp} from "firebase-admin/app";
import {getMessaging} from "firebase-admin/messaging";

initializeApp();

interface AlarmEventPayload {
  state?: string;
  event?: string;
  ts?: number | string;
}

export const notifyAlarmEvent = onValueCreated(
  {
    ref: "/devices/{deviceId}/alarm/{eventId}",
    region: "southamerica-east1",
  },
  async (event) => {
    const deviceId = event.params.deviceId as string;
    const data = event.data.val() as AlarmEventPayload | null;

    const state = data?.state ?? "ALERTA";
    const ev = data?.event ?? "ALARM";
    const ts = String(data?.ts ?? "");

    const topic = `device_${deviceId}`;

    await getMessaging().send({
      topic,
      notification: {
        title: `BitDogLab ${deviceId}`,
        body: `${state} (${ev})`,
      },
      data: {
        deviceId,
        eventId: event.params.eventId as string,
        state: String(state),
        event: String(ev),
        ts,
      },
      android: {
        priority: "high",
      },
      apns: {
        headers: {
          "apns-priority": "10",
        },
      },
    });
  },
);
