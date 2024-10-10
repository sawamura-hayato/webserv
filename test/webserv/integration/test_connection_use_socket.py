import socket
import unittest
from http.client import HTTPConnection, HTTPException
from typing import Optional

from common_functions import SERVER_PORT, assert_response
from common_response import (response_header_get_root_200_keep,
                             root_index_file, timeout_response)

# serverのtimeout+αを設定する
TIMEOUT = 4.0
BUFFER_SIZE = 1024


def receive_with_timeout(sock) -> Optional[str]:
    # timeoutを設定
    sock.settimeout(TIMEOUT)

    # timeout秒までに受信したデータをためておく
    received_data = []
    try:
        # 一度で全てrecvできない場合用にwhile
        while True:
            data = sock.recv(BUFFER_SIZE).decode("utf-8")
            if not data:
                # 受信データを繋げてstrにして返す
                return "".join(received_data)
            received_data.append(data)
    except socket.timeout:
        # timeout秒待っても何も送られてこなかった場合
        return None


# ------------------------------------------------------------------------------
# connect()してsend(),recv()を使うテスト
class TestConnectionUseSocket(unittest.TestCase):
    def setUp(self):
        # 各テストの前に実行される(unittestの機能)
        self.con = HTTPConnection("localhost", SERVER_PORT)
        self.con.connect()

    def tearDown(self):
        # 各テストの後に実行される(unittestの機能)
        if self.con:
            self.con.close()

    # 通常のkeep-aliveを送信(各関数の説明付き)
    def test_01_timeout_keep_alive(self) -> None:
        try:
            # request送信
            self.con.sock.send(
                b"GET / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\n\r\n"
            )

            # serverからのresponseを読み込む
            response = receive_with_timeout(self.con.sock)
            if response is None:
                # timeout秒経過していたらNG
                raise AssertionError
            else:
                # serverが200 OKを返したことを確認
                assert_response(
                    response, response_header_get_root_200_keep + root_index_file
                )

            # 再度responseを読み込み
            next_response = receive_with_timeout(self.con.sock)
            if next_response is None:
                # timeout秒経過していたらNG
                raise AssertionError
            else:
                # timeout秒経過前にserverから何も送られてこなかったらOK(接続が切れたという判断はできないかも？)
                assert len(next_response) == 0

        except HTTPException as e:
            print(f"Request failed: {e}")
            raise AssertionError

    def test_02_timeout_incomplete_request(self) -> None:
        try:
            # 完全ではないrequestを送信
            self.con.sock.send(b"GET / HTTP/1.1\r\nHost: localhost\r\nConnectio")

            response = receive_with_timeout(self.con.sock)
            if response is None:
                raise AssertionError
            else:
                # serverがtimeout responseを返したことを確認
                assert_response(response, timeout_response)

        except HTTPException as e:
            print(f"Request failed: {e}")
            raise AssertionError

    def test_03_timeout_no_request(self) -> None:
        try:
            # requestを何も送らない

            response = receive_with_timeout(self.con.sock)
            if response is None:
                raise AssertionError
            else:
                # serverが何も送らず接続を切断したらOK
                assert len(response) == 0

        except HTTPException as e:
            print(f"Request failed: {e}")
            raise AssertionError

    # serverがその後も正常に動いていればOK
    def test_04_incomplete_request_disconnect(self) -> None:
        try:
            # 完全ではないrequestを送信
            self.con.sock.send(b"GET / HTTP/1.1\r\nHost: localhost\r\nConnectio")

            # 完全なリクエストを送る前に接続を切断
            self.con.close()

        except HTTPException as e:
            print(f"Request failed: {e}")
            raise AssertionError
