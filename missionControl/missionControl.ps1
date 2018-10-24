$baseUri = "http://192.168.4.1";

Class AltimeterState {
    [bool]$IsActive
    [string]$State
    [string]$LogSize
    [string]$Time
    [string]$Temperature
    [string]$Altitude
    [string]$BmpSensorOk
}

Class CsvEntry {
    [string]$Time
    [string]$Temperature
    [string]$Altitude
}

$altimeterContext = $null;
$lastResponse = "";

function InitializeAltimeterContext {
    $Global:altimeterContext.IsActive = $false;    
    $Global:altimeterContext.State = 0;
    $Global:altimeterContext.Temperature = 0;
    $Global:altimeterContext.Altitude = 0;
    $Global:altimeterContext.BmpSensorOk = "Unknown";
    $Global:altimeterContext.LogSize = 0;
    $Global:altimeterContext.Time = 0;
}

function ParseStateResponse([string]$stateResponseContents, [AltimeterState]$outContext) {
    $tokens = $stateResponseContents.Split([char]13);
    # state, log size, sea level, BMP sensor
    $outContext.State = $tokens[0].Split(' ')[1].Trim();
    $outContext.LogSize = $tokens[1].Split(' ')[1].Trim();
    $outContext.BmpSensorOk = $tokens[3].Split(' ')[2].Trim();
}

function ParseLiveResponse([string]$stateResponseContents, [AltimeterState]$outContext) {
    $tokens = $stateResponseContents.Split([char]13);
    # time, temp, altitude
    $outContext.Time = $tokens[0].Split(':')[1].Trim();
    $outContext.Temperature = $tokens[1].Split(':')[1].Trim();
    $outContext.Altitude = $tokens[2].Split(':')[1].Trim();
}

function ParseLogPage([string]$logContents, [System.Collections.ArrayList]$writeToLog) {
    if ([string]::IsNullOrWhiteSpace($logContents)) {
        return 0;
    }

    $lines = $logContents.Split([char]13, [System.StringSplitOptions]::RemoveEmptyEntries);

    # just in case something weird comes back from the response
    if ($lines.Count -eq 1 -and [string]::IsNullOrWhiteSpace($lines[0])) {
        return 0;
    }

    foreach ($line in $lines) {
        $tokens = $line.Split(',');

        if ($tokens.Length -eq 3) {
            $o = new-object CsvEntry;                
            $o.Time = $tokens[0].Trim();
            $o.Temperature = $tokens[1].Trim();
            $o.Altitude = $tokens[2].Trim();        

            $writeToLog.Add($o);
        }        
    }

    return $lines.Length;
}

function DoRefreshState {
    try {
        $stateResponse = Invoke-WebRequest -Uri "$baseUri/" -Method Get -TimeoutSec 5 -UseBasicParsing;
        $liveResponse = Invoke-WebRequest -Uri "$baseUri/api/live" -Method Get -TimeoutSec 5 -UseBasicParsing;

        ParseStateResponse $stateResponse.Content $altimeterContext;
        ParseLiveResponse $liveResponse.Content $altimeterContext;

        $Global:altimeterContext.IsActive = $true;
    }
    catch {
         $Global:altimeterContext.IsActive = $false;
    }
}

function DoStartLogging {
    try {
        $response = Invoke-WebRequest -Uri "$baseUri/api/log/start" -Method Post -TimeoutSec 5 -UseBasicParsing;
        $Global:lastResponse = $response.Content;
    }
    catch {
        $Global:altimeterContext.IsActive = $false;
    }
}

function DoStopLogging {
    try {
        $response = Invoke-WebRequest -Uri "$baseUri/api/log/stop" -Method Post -TimeoutSec 5 -UseBasicParsing;
        $Global:lastResponse = $response.Content;
    }
    catch {
        $Global:altimeterContext.IsActive = $false;
    }
}

function DoDownloadLogs {
    try {
        # Stop logging first check
        write-host "Checking for log fetch readiness...";

        $response = Invoke-WebRequest -Uri "$baseUri/api/log/fetch?page=0" -Method Get -TimeoutSec 5 -UseBasicParsing;
        if ($response.Content.Contains("Stop logging first")) {
            $Global:lastResponse = $response.Content;
        }
        else {
            write-host "Downloading logs...";
            $csvLog = New-Object System.Collections.ArrayList;
            $hasMorePages = $true;
            $pageNumber = 0;

            do {
                $linesParsed = ParseLogPage $response.Content $csvLog

                if ($linesParsed -eq 0) {
                    write-host "No more data in subsequent pages.";
                    $hasMorePages = $false;
                }
                else {
                    $pageNumber++;

                    write-host "Fetching page $pageNumber...";
                    $response = Invoke-WebRequest -Uri "$baseUri/api/log/fetch?page=$pageNumber" -Method Get -TimeoutSec 5 -UseBasicParsing;                    
                }
            }
            while ($hasMorePages);

            write-host "Saving...";
            $timeStamp = [DateTime]::Now.ToString("yyyy-MM-dd_Thh_mm_ss");
            $fileName = "AltimeterLog_$timeStamp.csv";
            $path = "./$fileName";
            $downloadedLines = $csvLog.Count;
            
            $csvLog | Export-Csv -path $path -NoTypeInformation
            $Global:lastResponse = "Downloaded $downloadedLines lines to $path";
        }
    }
    catch {
        $Global:altimeterContext.IsActive = $false;
    }
}

function DoSetSeaLevelPressure {
    try {
        $seaLevelPressure = Read-Host "Enter sea level pressure hPa/mbar: ";
        $temp = 0;
        if ([string]::IsNullOrWhiteSpace($seaLevelPressure) -or [Float]::TryParse($seaLevelPressure, [ref]$temp) -eq $false) {
            $Global:lastResponse = "CLIENT: Blank or invalid value entered.";
        }
        else {
            $response = Invoke-WebRequest -Uri "$baseUri/api/config/localSeaLevelPressure?value=$seaLevelPressure" -Method Post -TimeoutSec 5 -UseBasicParsing;
            $Global:lastResponse = $response.Content;
        }
    }
    catch {
        $Global:altimeterContext.IsActive = $false;
    }
}

function ShowMainMenu {    
    cls;

    write-host "Mission Control";
    write-host "---------------------------------------";

    if ($Global:altimeterContext -ne $null) {
        if ($Global:altimeterContext.IsActive) {
            write-host "LATEST DATA:";
        }
        else {
            write-host "LAST SEEN DATA (DEVICE NOT ACTIVELY DETECTED)";
        }
        write-host "---------------------------------------";
        write-host "";
        write-host "Time: T+" $Global:altimeterContext.Time;
        write-host "State: " $Global:altimeterContext.State;
        write-host "BMP Sensor: " $Global:altimeterContext.BmpSensorOk;
        write-host "";
        write-host "Temp (C): " $Global:altimeterContext.Temperature;
        write-host "Altitude (m): " $Global:altimeterContext.Altitude;
        write-host "Log Size: " $Global:altimeterContext.LogSize;
        write-host ""
        write-host "---------------------------------------";        
    }
    else {
        write-host "Altimeter not detected, use REFRESH STATE to attempt to fetch state.";
    }

    if ([string]::IsNullOrWhiteSpace($Global:lastResponse) -eq $false) {
        write-host "COMMAND RESPONSE:";
        write-host $Global:lastResponse;
        write-host "---------------------------------------";
    }

    write-host "";
    write-host "Actions:";
    write-host "1. Refresh state";

    if ($Global:altimeterContext -ne $null) {
        write-host "2. Start logging";
        write-host "3. Stop logging";
        write-host "4. Download logs";
        write-host "5. Set Sea Level Pressure";
    }

    write-host "0. Exit";
    write-host "";    
}

do
{
    ShowMainMenu;
    $input = read-host "> ";

    if ($Global:altimeterContext -eq $null) {
        $Global:altimeterContext = New-Object AltimeterState;
        InitializeAltimeterContext;
    }

    switch ($input) {
        '1' {
            DoRefreshState
        }

        '2' {
            if ($Global:altimeterContext -ne $null) {
                DoStartLogging;
                DoRefreshState;
            }            
        }

        '3' {
            if ($Global:altimeterContext -ne $null) {
                DoStopLogging;
                DoRefreshState;
            }
        }

        '4' {
            if ($Global:altimeterContext -ne $null) {
                DoDownloadLogs;
                DoRefreshState;
            }
        }

        '5' {
            if ($Global:altimeterContext -ne $null) {
                DoSetSeaLevelPressure;
                DoRefreshState;
            }
        }
    }

}
until($input -eq '0');