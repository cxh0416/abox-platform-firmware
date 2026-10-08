#ifndef ABOX_ENROLLMENT_CA_PEM_H
#define ABOX_ENROLLMENT_CA_PEM_H
/* Public trust anchor only. Dedicated enrollment CA v1; never used by OTA.
 * SHA-256 certificate fingerprint:
 * A3A95EF878031B77C9BF29BF9D57D630757F99B031F90A16513B40C100856965
 * CA signing/private leaf keys remain in protected server storage. */
static const unsigned char abox_enrollment_ca_pem[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIDJzCCAg+gAwIBAgIUCzpPTTEJ5heJIsJ+Cht5YE1L1V4wDQYJKoZIhvcNAQEL\n"
"BQAwKjEoMCYGA1UEAwwfQUJveCBkZWRpY2F0ZWQgZW5yb2xsbWVudCBDQSB2MTAe\n"
"Fw0yNjEwMDYxNzUzNTdaFw0zNjEwMDUxNzUzNTdaMCoxKDAmBgNVBAMMH0FCb3gg\n"
"ZGVkaWNhdGVkIGVucm9sbG1lbnQgQ0EgdjEwggEiMA0GCSqGSIb3DQEBAQUAA4IB\n"
"DwAwggEKAoIBAQCZ+1fJ26LHjKhiW+ItzVWfhVInSKnFNVwSo0/90hNugRczihg+\n"
"HxB89rFQBb/+3dCzKqFD/IwEvMjd2iEVJrHYerThG0aoQYzKtZ2+0Ge9VA/AEeK2\n"
"lbQvBxEQGcyvwrjnZ4ylHJHPbfYlTJPgtdnykSVCDGvO+0mDwkiBgll36u9ucm3F\n"
"1Al/LcKdKjvfXDVLIvOse2xi2TD3s1o9r92guw3mhX3g/parsLx96Xh6RKV5G6Nn\n"
"PuICmW9iUIWbZAiQ7guRlTjQdAXweLqu6PgdRQ2UljIQ8HdYK+3QL40hDG57Tcdx\n"
"GvK4SHs0GXtJpjsuEp41Yy6z3WdqwZF9tj0zAgMBAAGjRTBDMBIGA1UdEwEB/wQI\n"
"MAYBAf8CAQAwDgYDVR0PAQH/BAQDAgEGMB0GA1UdDgQWBBSHu9iF8OWoKsHZbb7N\n"
"aRDZ1NtmXTANBgkqhkiG9w0BAQsFAAOCAQEALPSgs/thbpj+pEL0qnUZ63qbPIAW\n"
"rKuoLCiS+HRp1HEKhJhBkGsN6PndLCz9OLrOpWUTl2WkYesdlj3iVOu3e4U1c3lI\n"
"tlT44uqQMDO+00yd0GAijw1C/uIcw55bEpFA9JvxHXRjg3eumm22PjcpNbgjEUM/\n"
"4GU73NiJPI6vknWajyng1+tSPM4eYLARX4+FDwCm/0gPmbiYe/fokrxEueoiAR2Z\n"
"CEjeq1/hpOClJkYwoyeSGaWptkCyt/FnO92D1YNW/9MkWSnSB5iPd1hLLG/Kd02Y\n"
"CZUv/F8MwN7pEI2DT1jCpuF3bu+YUa8CzMNkM4vGiA3c8uuQDd/6bbcnHw==\n"
"-----END CERTIFICATE-----\n";
#endif
