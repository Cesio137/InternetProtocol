# Sender.ps1
param (
    [string]$message = "Olá, servidor",
    [string]$server = "127.0.0.1",
    [int]$port = 3000
)

$udpClient = New-Object System.Net.Sockets.UdpClient
$udpClient.Connect($server, $port)

$encodedMessage = [System.Text.Encoding]::UTF8.GetBytes($message)
$udpClient.Send($encodedMessage, $encodedMessage.Length)

$remoteEndPoint = New-Object System.Net.IPEndPoint ([System.Net.IPAddress]::Any, 0)
$responseBytes = $udpClient.Receive([ref]$remoteEndPoint)
$response = [System.Text.Encoding]::UTF8.GetString($responseBytes)

Write-Host "Received response: $response"

$udpClient.Close()
