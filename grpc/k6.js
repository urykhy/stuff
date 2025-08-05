import exec from 'k6/execution';
import { check } from 'k6';
import { Client, StatusOK } from 'k6/net/grpc';

const client = new Client();
client.load(['.'], 'Play.proto');

export const options = {
    scenarios: {
        example_scenario: {
            executor: 'constant-vus',
            gracefulStop: '1s',
            vus: 10,
            duration: '2s',
        }
    },
};

export default function () {
    if (exec.vu.iterationInScenario == 0) {
        client.connect('127.0.0.1:56780', { 'plaintext': true });
    }
    const data = { value: 1 };
    const response = client.invoke('play.PlayService/Ping', data);
    check(response, {
        'status is OK': (r) => r && r.status === StatusOK,
    });
}
