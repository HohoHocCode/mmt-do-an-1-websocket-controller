#include "C:\UNIVERSITY\ComputerNetwork\remote-desktop-control\server\include\network\web_server.h"

// Implementation of WebServer class
std::string WebServer::urlDecode(const std::string &str)
{
  std::string result;
  result.reserve(str.size());

  for (size_t i = 0; i < str.size(); ++i)
  {
    if (str[i] == '%')
    {
      if (i + 2 < str.size())
      {
        std::string hex = str.substr(i + 1, 2);
        char ch = static_cast<char>(std::stoi(hex, nullptr, 16));
        result += ch;
        i += 2;
      }
      else
      {
        result += '%';
      }
    }
    else if (str[i] == '+')
    {
      result += ' ';
    }
    else
    {
      result += str[i];
    }
  }

  return result;
}
// Implementation
WebServer::WebServer(int port) : port(port), serverSock(-1), running(false)
{
  platform = Platform::createPlatform();
  cmdHandler = std::make_unique<CommandHandler>();
  logFile = "remote_access.log";
  platform->initSockets();
}

WebServer::~WebServer()
{
  stop();
  platform->cleanupSockets();
}

bool WebServer::start()
{
  serverSock = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSock == INVALID_SOCKET)
    return false;

  int opt = 1;
  setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(serverSock, (sockaddr *)&addr, sizeof(addr)) < 0)
  {
    platform->closeSocket(serverSock);
    return false;
  }

  if (listen(serverSock, 10) < 0)
  {
    platform->closeSocket(serverSock);
    return false;
  }

  running = true;
  return true;
}

void WebServer::stop()
{
  running = false;
  if (serverSock != INVALID_SOCKET)
  {
    platform->closeSocket(serverSock);
    serverSock = -1;
  }
}

void WebServer::run()
{
  std::cout << "Web server started on http://localhost:" << port << "\n";

  while (running)
  {
    sockaddr_in client{};
    socklen_t len = sizeof(client);
    SocketHandle clientSock = accept(serverSock, (sockaddr *)&client, &len);

    if (clientSock == INVALID_SOCKET)
    {
      if (running)
      {
        std::cerr << "Failed to accept client connection\n";
      }
      continue;
    }

    char clientIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client.sin_addr, clientIP, INET_ADDRSTRLEN);
    std::string clientAddr = clientIP;

    // Read request
    char buffer[65536];
    int bytes = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0)
    {
      buffer[bytes] = '\0';
      std::string request(buffer);

      std::string response = handleRequest(request, clientAddr);
      send(clientSock, response.c_str(), response.size(), 0);
    }

    platform->closeSocket(clientSock);
  }
}

std::string WebServer::handleRequest(const std::string &request, const std::string &clientAddr)
{
  std::string method = parseHTTPMethod(request);
  std::string path = parseHTTPPath(request);
  std::string body = parseHTTPBody(request);

  // Serve static files
  if (path == "/" || path == "/index.html")
  {
    return buildHTTPResponse(200, "text/html", getIndexHTML());
  }

  // API endpoints
  if (path.find("/api/") == 0)
  {
    return handleAPI(path, method, body, clientAddr);
  }

  return buildHTTPResponse(404, "text/plain", "Not Found");
}

std::string WebServer::handleAPI(const std::string &path, const std::string &method,
                                 const std::string &body, const std::string &clientAddr)
{
  (void)method;     // ← Add this
  (void)clientAddr; // ← Add this

  // Extract session ID from headers or body
  std::string sessionId;
  if (body.find("sessionId") != std::string::npos)
  {
    size_t pos = body.find("\"sessionId\":\"");
    if (pos != std::string::npos)
    {
      pos += 13;
      size_t end = body.find("\"", pos);
      sessionId = body.substr(pos, end - pos);
    }
  }

  try
  {
    if (path == "/api/login")
    {
      return handleLogin(body);
    }
    else if (path == "/api/command")
    {
      return handleCommand(body, sessionId);
    }
    else if (path == "/api/processes")
    {
      return handleProcessList(sessionId);
    }
    else if (path == "/api/sysinfo")
    {
      return handleSystemInfo(sessionId);
    }
    else if (path == "/api/history")
    {
      return handleHistory(sessionId);
    }
    else if (path == "/api/logs")
    {
      return handleLogs();
    }

    return buildErrorResponse("Unknown API endpoint");
  }
  catch (const std::exception &e)
  {
    return buildErrorResponse(e.what());
  }
}

std::string WebServer::handleLogin(const std::string &body)
{
  // Parse username from JSON body
  size_t pos = body.find("\"username\":\"");
  std::string username = "anonymous";
  if (pos != std::string::npos)
  {
    pos += 12;
    size_t end = body.find("\"", pos);
    username = body.substr(pos, end - pos);
  }

  std::string sessionId = createSession(username);
  auto sysInfo = platform->getSystemInfo();

  std::ostringstream json;
  json << "{\"success\":true,\"sessionId\":\"" << sessionId << "\","
       << "\"serverInfo\":{\"os\":\"" << sysInfo.osName << "\","
       << "\"hostname\":\"" << sysInfo.hostname << "\","
       << "\"cpuCores\":" << sysInfo.cpuCores << ","
       << "\"totalMemory\":" << sysInfo.totalMemory << "}}";

  return buildJSONResponse(json.str());
}

std::string WebServer::handleCommand(const std::string &body, const std::string &sessionId)
{
  if (!validateSession(sessionId))
  {
    return buildErrorResponse("Invalid session");
  }

  // Parse command from JSON
  size_t pos = body.find("\"command\":\"");
  if (pos == std::string::npos)
  {
    return buildErrorResponse("No command provided");
  }

  pos += 11;
  size_t end = body.find("\"", pos);
  std::string command = urlDecode(body.substr(pos, end - pos));

  // Execute command
  std::string result = cmdHandler->execute(command);

  // Log the command
  Session &session = sessions[sessionId];
  logCommand(sessionId, session.username, command, result);
  updateSessionActivity(sessionId);

  std::ostringstream json;
  json << "{\"success\":true,\"result\":\"" << escapeJSON(result) << "\"}";

  return buildJSONResponse(json.str());
}

std::string WebServer::handleProcessList(const std::string &sessionId)
{
  if (!validateSession(sessionId))
  {
    return buildErrorResponse("Invalid session");
  }

  auto processes = platform->listProcesses();
  updateSessionActivity(sessionId);

  std::ostringstream json;
  json << "{\"success\":true,\"processes\":[";

  bool first = true;
  int count = 0;
  for (const auto &proc : processes)
  {
    if (proc.name.empty() || proc.name == "<unknown>")
      continue;
    if (count++ >= 100)
      break; // Limit to 100 processes

    if (!first)
      json << ",";
    first = false;

    json << "{\"pid\":" << proc.pid << ","
         << "\"name\":\"" << escapeJSON(proc.name) << "\","
         << "\"memory\":" << proc.memoryUsage << "}";
  }

  json << "]}";
  return buildJSONResponse(json.str());
}

std::string WebServer::handleSystemInfo(const std::string &sessionId)
{
  if (!validateSession(sessionId))
  {
    return buildErrorResponse("Invalid session");
  }

  auto info = platform->getSystemInfo();
  updateSessionActivity(sessionId);

  std::ostringstream json;
  json << "{\"success\":true,\"system\":{"
       << "\"os\":\"" << info.osName << "\","
       << "\"arch\":\"" << info.architecture << "\","
       << "\"hostname\":\"" << info.hostname << "\","
       << "\"cpuCores\":" << info.cpuCores << ","
       << "\"totalMemory\":" << info.totalMemory << ","
       << "\"availableMemory\":" << info.availableMemory << "}}";

  return buildJSONResponse(json.str());
}

std::string WebServer::handleHistory(const std::string &sessionId)
{
  if (!validateSession(sessionId))
  {
    return buildErrorResponse("Invalid session");
  }

  Session &session = sessions[sessionId];

  std::ostringstream json;
  json << "{\"success\":true,\"history\":[";

  bool first = true;
  for (const auto &cmd : session.commandHistory)
  {
    if (!first)
      json << ",";
    first = false;
    json << "\"" << escapeJSON(cmd) << "\"";
  }

  json << "]}";
  return buildJSONResponse(json.str());
}

std::string WebServer::handleLogs()
{
  std::ifstream file(logFile);
  if (!file)
  {
    return buildErrorResponse("Cannot read log file");
  }

  std::ostringstream json;
  json << "{\"success\":true,\"logs\":\"";

  std::string line;
  std::vector<std::string> lines;
  while (std::getline(file, line))
  {
    lines.push_back(line);
  }

  // Return last 100 lines
  int start = lines.size() > 100 ? lines.size() - 100 : 0;
  for (size_t i = start; i < lines.size(); i++)
  {
    json << escapeJSON(lines[i]) << "\\n";
  }

  json << "\"}";
  return buildJSONResponse(json.str());
}

// Helper implementations
std::string WebServer::createSession(const std::string &username)
{
  std::lock_guard<std::mutex> lock(sessionMutex);

  Session session;
  session.sessionId = generateSessionId();
  session.username = username;
  session.createdAt = std::time(nullptr);
  session.lastActivity = session.createdAt;

  sessions[session.sessionId] = session;
  return session.sessionId;
}

bool WebServer::validateSession(const std::string &sessionId)
{
  std::lock_guard<std::mutex> lock(sessionMutex);
  return sessions.find(sessionId) != sessions.end();
}

void WebServer::updateSessionActivity(const std::string &sessionId)
{
  std::lock_guard<std::mutex> lock(sessionMutex);
  if (sessions.find(sessionId) != sessions.end())
  {
    sessions[sessionId].lastActivity = std::time(nullptr);
  }
}

void WebServer::logCommand(const std::string &sessionId, const std::string &username,
                           const std::string &command, const std::string &result)
{
  std::lock_guard<std::mutex> lock(logMutex);

  // Add to session history
  if (sessions.find(sessionId) != sessions.end())
  {
    sessions[sessionId].commandHistory.push_back(command);
    if (sessions[sessionId].commandHistory.size() > 50)
    {
      sessions[sessionId].commandHistory.erase(
          sessions[sessionId].commandHistory.begin());
    }
  }

  // Write to log file
  std::ofstream file(logFile, std::ios::app);
  if (file)
  {
    file << getCurrentTimestamp() << " | "
         << "User: " << username << " | "
         << "Session: " << sessionId << " | "
         << "Command: " << command << " | "
         << "Result: " << (result.length() > 100 ? result.substr(0, 100) + "..." : result) << "\n";
  }
}

std::string WebServer::generateSessionId()
{
  static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::string id;
  for (int i = 0; i < 32; i++)
  {
    id += alphanum[rand() % (sizeof(alphanum) - 1)];
  }
  return id;
}

std::string WebServer::getCurrentTimestamp()
{
  auto now = std::time(nullptr);
  auto tm = std::localtime(&now);
  std::ostringstream oss;
  oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
  return oss.str();
}

std::string WebServer::buildHTTPResponse(int code, const std::string &contentType,
                                         const std::string &body)
{
  std::ostringstream response;
  response << "HTTP/1.1 " << code << " OK\r\n"
           << "Content-Type: " << contentType << "\r\n"
           << "Content-Length: " << body.size() << "\r\n"
           << "Access-Control-Allow-Origin: *\r\n"
           << "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n" // Added for robustness
           << "Access-Control-Allow-Headers: Content-Type\r\n"       // Added for robustness
           << "Connection: close\r\n\r\n"
           << body;
  return response.str();
}

std::string WebServer::buildJSONResponse(const std::string &json)
{
  return buildHTTPResponse(200, "application/json", json);
}

std::string WebServer::buildErrorResponse(const std::string &error)
{
  std::ostringstream json;
  json << "{\"success\":false,\"error\":\"" << escapeJSON(error) << "\"}";
  return buildJSONResponse(json.str());
}

std::string WebServer::escapeJSON(const std::string &str)
{
  std::string result;
  for (char c : str)
  {
    switch (c)
    {
    case '"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default:
      if (c >= 0 && c <= 31)
      {
        // Don't include control characters
      }
      else
      {
        result += c;
      }
    }
  }
  return result;
}

// Parse helpers (simplified implementations)
std::string WebServer::parseHTTPMethod(const std::string &request)
{
  size_t end = request.find(' ');
  return request.substr(0, end);
}

std::string WebServer::parseHTTPPath(const std::string &request)
{
  size_t start = request.find(' ') + 1;
  size_t end = request.find(' ', start);
  std::string fullPath = request.substr(start, end - start);
  size_t queryPos = fullPath.find('?');
  return queryPos != std::string::npos ? fullPath.substr(0, queryPos) : fullPath;
}

std::string WebServer::parseHTTPBody(const std::string &request)
{
  size_t pos = request.find("\r\n\r\n");
  return pos != std::string::npos ? request.substr(pos + 4) : "";
}

// *** ADDED IMPLEMENTATION ***
std::string WebServer::getIndexHTML()
{
  // This C++ raw string literal contains your exact index_html.html file.
  return R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Remote Desktop Control - Web Interface</title>
    
    <script src="https://cdn.tailwindcss.com"></script>
    
    <script crossorigin src="https://unpkg.com/react@18/umd/react.production.min.js"></script>
    <script crossorigin src="https://unpkg.com/react-dom@18/umd/react-dom.production.min.js"></script>
    
    <script src="https://unpkg.com/@babel/standalone/babel.min.js"></script>
    
    <style>
        body {
            margin: 0;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', 'Roboto', 'Oxygen',
                'Ubuntu', 'Cantarell', 'Fira Sans', 'Droid Sans', 'Helvetica Neue',
                sans-serif;
            -webkit-font-smoothing: antialiased;
            -moz-osx-font-smoothing: grayscale;
        }
        
        code {
            font-family: source-code-pro, Menlo, Monaco, Consolas, 'Courier New', monospace;
        }
        
        /* Custom scrollbar */
        ::-webkit-scrollbar {
            width: 8px;
            height: 8px;
        }
        
        ::-webkit-scrollbar-track {
            background: rgba(30, 41, 59, 0.5);
            border-radius: 4px;
        }
        
        ::-webkit-scrollbar-thumb {
            background: rgba(139, 92, 246, 0.5);
            border-radius: 4px;
        }
        
        ::-webkit-scrollbar-thumb:hover {
            background: rgba(139, 92, 246, 0.7);
        }
    </style>
</head>
<body>
    <div id="root"></div>
    
    <script type="text/babel">
        const { useState, useEffect, useRef } = React;
        
        // Icon components (simplified Lucide icons)
        const Terminal = () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <polyline points="4 17 10 11 4 5"></polyline>
                <line x1="12" y1="19" x2="20" y2="19"></line>
            </svg>
        );
        
        const Server = () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <rect x="2" y="2" width="20" height="8" rx="2" ry="2"></rect>
                <rect x="2" y="14" width="20" height="8" rx="2" ry="2"></rect>
                <line x1="6" y1="6" x2="6.01" y2="6"></line>
                <line x1="6" y1="18" x2="6.01" y2="18"></line>
            </svg>
        );
        
        const Cpu = () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <rect x="4" y="4" width="16" height="16" rx="2" ry="2"></rect>
                <rect x="9" y="9" width="6" height="6"></rect>
                <line x1="9" y1="1" x2="9" y2="4"></line>
                <line x1="15" y1="1" x2="15" y2="4"></line>
                <line x1="9" y1="20" x2="9" y2="23"></line>
                <line x1="15" y1="20" x2="15" y2="23"></line>
                <line x1="20" y1="9" x2="23" y2="9"></line>
                <line x1="20" y1="14" x2="23" y2="14"></line>
                <line x1="1" y1="9" x2="4" y2="9"></line>
                <line x1="1" y1="14" x2="4" y2="14"></line>
            </svg>
        );
        
        const Activity = () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <polyline points="22 12 18 12 15 21 9 3 6 12 2 12"></polyline>
            </svg>
        );
        
        const History = () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <path d="M3 3v5h5"></path>
                <path d="M3.05 13A9 9 0 1 0 6 5.3L3 8"></path>
                <path d="M12 7v5l4 2"></path>
            </svg>
        );
        
        const Command = () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <path d="M18 3a3 3 0 0 0-3 3v12a3 3 0 0 0 3 3 3 3 0 0 0 3-3 3 3 0 0 0-3-3H6a3 3 0 0 0-3 3 3 3 0 0 0 3 3 3 3 0 0 0 3-3V6a3 3 0 0 0-3-3 3 3 0 0 0-3 3 3 3 0 0 0 3 3h12a3 3 0 0 0 3-3 3 3 0 0 0-3-3z"></path>
            </svg>
        );
        
        const LogOut = () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"></path>
                <polyline points="16 17 21 12 16 7"></polyline>
                <line x1="21" y1="12" x2="9" y2="12"></line>
            </svg>
        );
        
        const RefreshCw = ({ className }) => (
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round" className={className}>
                <polyline points="23 4 23 10 17 10"></polyline>
                <polyline points="1 20 1 14 7 14"></polyline>
                <path d="M3.51 9a9 9 0 0 1 14.85-3.36L23 10M1 14l4.64 4.36A9 9 0 0 0 20.49 15"></path>
            </svg>
        );
        
        const Play = () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <polygon points="5 3 19 12 5 21 5 3"></polygon>
            </svg>
        );
        
        const X = () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <line x1="18" y1="6" x2="6" y2="18"></line>
                <line x1="6" y1="6" x2="18" y2="18"></line>
            </svg>
        );
        
        const Info = () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <circle cx="12" cy="12" r="10"></circle>
                <line x1="12" y1="16" x2="12" y2="12"></line>
                <line x1="12" y1="8" x2="12.01" y2="8"></line>
            </svg>
        );
        
        const HardDrive = () => (
            <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <line x1="22" y1="12" x2="2" y2="12"></line>
                <path d="M5.45 5.11L2 12v6a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-6l-3.45-6.89A2 2 0 0 0 16.76 4H7.24a2 2 0 0 0-1.79 1.11z"></path>
                <line x1="6" y1="16" x2="6.01" y2="16"></line>
                <line x1="10" y1="16" x2="10.01" y2="16"></line>
            </svg>
        );

        const RemoteDesktopControl = () => {
          const [isConnected, setIsConnected] = useState(false);
          const [sessionId, setSessionId] = useState('');
          const [username, setUsername] = useState('');
          const [serverInfo, setServerInfo] = useState(null);
          const [activeTab, setActiveTab] = useState('terminal');
          const [command, setCommand] = useState('');
          const [terminalOutput, setTerminalOutput] = useState([]);
          const [processes, setProcesses] = useState([]);
          const [systemInfo, setSystemInfo] = useState(null);
          const [commandHistory, setCommandHistory] = useState([]);
          const [logs, setLogs] = useState('');
          const [loading, setLoading] = useState(false);
          const [historyIndex, setHistoryIndex] = useState(-1);
          const terminalRef = useRef(null);

          const API_BASE = 'http://localhost:8080/api';

          useEffect(() => {
            if (terminalRef.current) {
              terminalRef.current.scrollTop = terminalRef.current.scrollHeight;
            }
          }, [terminalOutput]);

          const apiCall = async (endpoint, data = {}) => {
            try {
              const response = await fetch(`${API_BASE}${endpoint}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ ...data, sessionId })
              });
              return await response.json();
            } catch (error) {
              console.error('API Error:', error);
              return { success: false, error: error.message };
            }
          };

          const handleLogin = async () => {
            if (!username.trim()) {
              alert('Please enter a username');
              return;
            }

            setLoading(true);
            const result = await apiCall('/login', { username });
            setLoading(false);

            if (result.success) {
              setSessionId(result.sessionId);
              setServerInfo(result.serverInfo);
              setIsConnected(true);
              addToTerminal(`Connected to ${result.serverInfo.hostname} (${result.serverInfo.os})`, 'success');
            } else {
              alert('Connection failed: ' + result.error);
            }
          };

          const handleCommand = async (cmd) => {
            if (!cmd.trim()) return;

            addToTerminal(`$ ${cmd}`, 'command');
            setLoading(true);

            const result = await apiCall('/command', { command: cmd });
            setLoading(false);

            if (result.success) {
              addToTerminal(result.result, 'output');
              setCommandHistory(prev => [...prev, cmd]);
            } else {
              addToTerminal(`Error: ${result.error}`, 'error');
            }

            setCommand('');
            setHistoryIndex(-1);
          };

          const loadProcesses = async () => {
            setLoading(true);
            const result = await apiCall('/processes');
            setLoading(false);
            if (result.success) setProcesses(result.processes);
          };

          const loadSystemInfo = async () => {
            setLoading(true);
            const result = await apiCall('/sysinfo');
            setLoading(false);
            if (result.success) setSystemInfo(result.system);
          };

          const loadHistory = async () => {
            const result = await apiCall('/history');
            if (result.success) setCommandHistory(result.history);
          };

          const loadLogs = async () => {
            setLoading(true);
            const result = await apiCall('/logs');
            setLoading(false);
            if (result.success) setLogs(result.logs);
          };

          const addToTerminal = (text, type = 'output') => {
            setTerminalOutput(prev => [...prev, { text, type, timestamp: new Date() }]);
          };

          const handleKeyDown = (e) => {
            if (e.key === 'Enter') {
              handleCommand(command);
            } else if (e.key === 'ArrowUp') {
              e.preventDefault();
              if (historyIndex < commandHistory.length - 1) {
                const newIndex = historyIndex + 1;
                setHistoryIndex(newIndex);
                setCommand(commandHistory[commandHistory.length - 1 - newIndex]);
              }
            } else if (e.key === 'ArrowDown') {
              e.preventDefault();
              if (historyIndex > 0) {
                const newIndex = historyIndex - 1;
                setHistoryIndex(newIndex);
                setCommand(commandHistory[commandHistory.length - 1 - newIndex]);
              } else if (historyIndex === 0) {
                setHistoryIndex(-1);
                setCommand('');
              }
            }
          };

          const killProcess = async (pid) => {
            if (confirm(`Kill process ${pid}?`)) {
              await handleCommand(`kill ${pid}`);
              setTimeout(loadProcesses, 1000);
            }
          };

          const quickCommands = [
            { label: 'List Processes', cmd: 'list' },
            { label: 'System Info', cmd: 'sysinfo' },
            { label: 'Help', cmd: 'help' },
            { label: 'List Directory', cmd: 'ls .' }
          ];

          useEffect(() => {
            if (!isConnected) return;
            if (activeTab === 'processes') loadProcesses();
            else if (activeTab === 'system') loadSystemInfo();
            else if (activeTab === 'history') loadHistory();
            else if (activeTab === 'logs') loadLogs();
          }, [activeTab, isConnected]);

          if (!isConnected) {
            return (
              <div className="min-h-screen bg-gradient-to-br from-slate-900 via-purple-900 to-slate-900 flex items-center justify-center p-4">
                <div className="bg-slate-800/50 backdrop-blur-xl rounded-2xl shadow-2xl p-8 w-full max-w-md border border-purple-500/20">
                  <div className="flex items-center justify-center mb-6">
                    <div className="w-16 h-16 text-purple-400 flex items-center justify-center">
                      <Server />
                    </div>
                  </div>
                  <h1 className="text-3xl font-bold text-white text-center mb-2">
                    Remote Desktop Control
                  </h1>
                  <p className="text-slate-400 text-center mb-8">
                    Connect to your remote server
                  </p>
                  
                  <div className="space-y-4">
                    <div>
                      <label className="block text-sm font-medium text-slate-300 mb-2">
                        Username
                      </label>
                      <input
                        type="text"
                        value={username}
                        onChange={(e) => setUsername(e.target.value)}
                        onKeyDown={(e) => e.key === 'Enter' && handleLogin()}
                        className="w-full px-4 py-3 bg-slate-700/50 border border-slate-600 rounded-lg text-white placeholder-slate-400 focus:outline-none focus:ring-2 focus:ring-purple-500"
                        placeholder="Enter your username"
                      />
                    </div>
                    
                    <button
                      onClick={handleLogin}
                      disabled={loading}
                      className="w-full bg-gradient-to-r from-purple-500 to-pink-500 text-white py-3 rounded-lg font-semibold hover:from-purple-600 hover:to-pink-600 transition-all disabled:opacity-50 disabled:cursor-not-allowed flex items-center justify-center gap-2"
                    >
                      {loading ? (
                        <>
                          <RefreshCw className="animate-spin" />
                          <span>Connecting...</span>
                        </>
                      ) : (
                        <>
                          <Server />
                          <span>Connect</span>
                        </>
                      )}
                    </button>
                  </div>
                </div>
              </div>
            );
          }

          // Main interface when connected
          return (
            <div className="min-h-screen bg-gradient-to-br from-slate-900 via-purple-900 to-slate-900 p-4">
              <div className="max-w-7xl mx-auto">
                {/* Header */}
                <div className="bg-slate-800/50 backdrop-blur-xl rounded-2xl shadow-2xl p-4 mb-4 border border-purple-500/20">
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-4">
                      <div className="w-8 h-8 text-purple-400"><Server /></div>
                      <div>
                        <h1 className="text-xl font-bold text-white">
                          {serverInfo?.hostname || 'Remote Server'}
                        </h1>
                        <p className="text-sm text-slate-400">
                          {serverInfo?.os} • {username} • Connected
                        </p>
                      </div>
                    </div>
                    <button
                      onClick={() => {
                        setIsConnected(false);
                        setSessionId('');
                        setTerminalOutput([]);
                      }}
                      className="flex items-center gap-2 px-4 py-2 bg-red-500/20 text-red-400 rounded-lg hover:bg-red-500/30 transition-colors"
                    >
                      <div className="w-4 h-4"><LogOut /></div>
                      <span>Disconnect</span>
                    </button>
                  </div>
                </div>

                {/* Tabs */}
                <div className="bg-slate-800/50 backdrop-blur-xl rounded-2xl shadow-2xl mb-4 border border-purple-500/20">
                  <div className="flex gap-2 p-2 overflow-x-auto">
                    {[
                      { id: 'terminal', label: 'Terminal', Icon: Terminal },
                      { id: 'processes', label: 'Processes', Icon: Activity },
                      { id: 'system', label: 'System', Icon: Cpu },
                      { id: 'history', label: 'History', Icon: History },
                      { id: 'logs', label: 'Logs', Icon: Command }
                    ].map(tab => (
                      <button
                        key={tab.id}
                        onClick={() => setActiveTab(tab.id)}
                        className={`flex items-center gap-2 px-4 py-2 rounded-lg transition-all ${
                          activeTab === tab.id
                            ? 'bg-purple-500 text-white'
                            : 'text-slate-400 hover:bg-slate-700/50'
                        }`}
                      >
                        <div className="w-4 h-4"><tab.Icon /></div>
                        <span>{tab.label}</span>
                      </button>
                    ))}
                  </div>
                </div>

                {/* Content */}
                <div className="bg-slate-800/50 backdrop-blur-xl rounded-2xl shadow-2xl p-6 border border-purple-500/20">
                  {activeTab === 'terminal' && (
                    <div className="space-y-4">
                      <div className="flex flex-wrap gap-2 mb-4">
                        {quickCommands.map(qc => (
                          <button
                            key={qc.cmd}
                            onClick={() => handleCommand(qc.cmd)}
                            className="px-3 py-1 bg-purple-500/20 text-purple-300 rounded-lg text-sm hover:bg-purple-500/30 transition-colors"
                          >
                            {qc.label}
                          </button>
                        ))}
                      </div>

                      <div
                        ref={terminalRef}
                        className="bg-slate-900/80 rounded-lg p-4 h-96 overflow-y-auto font-mono text-sm"
                      >
                        {terminalOutput.map((line, i) => (
                          <div
                            key={i}
                            className={`mb-1 ${
                              line.type === 'command'
                                ? 'text-purple-400 font-bold'
                                : line.type === 'error'
                                ? 'text-red-400'
                                : line.type === 'success'
                                ? 'text-green-400'
                                : 'text-slate-300'
                            }`}
                            style={{ whiteSpace: 'pre-wrap' }}
                          >
                            {line.text}
                          </div>
                        ))}
                        {loading && (
                          <div className="text-purple-400 animate-pulse">Processing...</div>
                        )}
                      </div>

                      <div className="flex gap-2">
                        <input
                          type="text"
                          value={command}
                          onChange={(e) => setCommand(e.target.value)}
                          onKeyDown={handleKeyDown}
                          placeholder="Enter command... (↑↓ for history)"
                          className="flex-1 px-4 py-3 bg-slate-700/50 border border-slate-600 rounded-lg text-white placeholder-slate-400 focus:outline-none focus:ring-2 focus:ring-purple-500 font-mono"
                        />
                        <button
                          onClick={() => handleCommand(command)}
                          disabled={loading || !command.trim()}
                          className="px-6 py-3 bg-gradient-to-r from-purple-500 to-pink-500 text-white rounded-lg font-semibold hover:from-purple-600 hover:to-pink-600 transition-all disabled:opacity-50 disabled:cursor-not-allowed"
                        >
                          <div className="w-5 h-5"><Play /></div>
                        </button>
                      </div>
                    </div>
                  )}

                  {activeTab === 'processes' && (
                    <div className="space-y-4">
                      <div className="flex justify-between items-center mb-4">
                        <h2 className="text-xl font-bold text-white">Running Processes</h2>
                        <button
                          onClick={loadProcesses}
                          className="flex items-center gap-2 px-4 py-2 bg-purple-500/20 text-purple-300 rounded-lg hover:bg-purple-500/30"
                        >
                          <div className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`}><RefreshCw /></div>
                          <span>Refresh</span>
                        </button>
                      </div>

                      <div className="overflow-x-auto">
                        <table className="w-full text-left text-slate-300">
                          <thead className="bg-slate-700/50">
                            <tr>
                              <th className="px-4 py-3 rounded-tl-lg">PID</th>
                              <th className="px-4 py-3">Name</th>
                              <th className="px-4 py-3">Memory (KB)</th>
                              <th className="px-4 py-3 rounded-tr-lg">Actions</th>
                            </tr>
                          </thead>
                          <tbody>
                            {processes.map(proc => (
                              <tr key={proc.pid} className="border-t border-slate-700/50 hover:bg-slate-700/30">
                                <td className="px-4 py-3 font-mono">{proc.pid}</td>
                                <td className="px-4 py-3">{proc.name}</td>
                                <td className="px-4 py-3">{proc.memory.toLocaleString()}</td>
                                <td className="px-4 py-3">
                                  <button
                                    onClick={() => killProcess(proc.pid)}
                                    className="flex items-center gap-1 px-3 py-1 bg-red-500/20 text-red-400 rounded hover:bg-red-500/30 text-sm"
                                  >
                                    <div className="w-3 h-3"><X /></div>
                                    <span>Kill</span>
                                  </button>
                                </td>
                              </tr>
                            ))}
                          </tbody>
                        </table>
                      </div>
                    </div>
                  )}

                  {activeTab === 'system' && systemInfo && (
                    <div className="space-y-6">
                      <h2 className="text-xl font-bold text-white mb-4">System Information</h2>
                      
                      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                        <div className="bg-slate-700/30 rounded-lg p-4">
                          <div className="flex items-center gap-2 mb-2">
                            <div className="w-5 h-5 text-purple-400"><Server /></div>
                            <h3 className="font-semibold text-white">Operating System</h3>
                          </div>
                          <p className="text-2xl font-bold text-purple-400">{systemInfo.os}</p>
                          <p className="text-slate-400 text-sm">{systemInfo.arch}</p>
                        </div>

                        <div className="bg-slate-700/30 rounded-lg p-4">
                          <div className="flex items-center gap-2 mb-2">
                            <div className="w-5 h-5 text-blue-400"><Info /></div>
                            <h3 className="font-semibold text-white">Hostname</h3>
                          </div>
                          <p className="text-2xl font-bold text-blue-400">{systemInfo.hostname}</p>
                        </div>

                        <div className="bg-slate-700/30 rounded-lg p-4">
                          <div className="flex items-center gap-2 mb-2">
                            <div className="w-5 h-5 text-green-400"><Cpu /></div>
                            <h3 className="font-semibold text-white">CPU Cores</h3>
                          </div>
                          <p className="text-2xl font-bold text-green-400">{systemInfo.cpuCores}</p>
                        </div>

                        <div className="bg-slate-700/30 rounded-lg p-4">
                          <div className="flex items-center gap-2 mb-2">
                            <div className="w-5 h-5 text-yellow-400"><HardDrive /></div>
                            <h3 className="font-semibold text-white">Memory</h3>
                          </div>
                          <p className="text-2xl font-bold text-yellow-400">
                            {systemInfo.availableMemory} / {systemInfo.totalMemory} MB
                          </p>
                          <div className="mt-2 bg-slate-600/50 rounded-full h-2">
                            <div
                              className="bg-gradient-to-r from-yellow-400 to-orange-400 h-2 rounded-full"
                              style={{
                                width: `${((systemInfo.totalMemory - systemInfo.availableMemory) / systemInfo.totalMemory) * 100}%`
                              }}
                            />
                          </div>
                        </div>
                      </div>
                    </div>
                  )}

                  {activeTab === 'history' && (
                    <div className="space-y-4">
                      <h2 className="text-xl font-bold text-white mb-4">Command History</h2>
                      <div className="bg-slate-900/80 rounded-lg p-4 max-h-96 overflow-y-auto">
                        {commandHistory.length === 0 ? (
                          <p className="text-slate-400 text-center py-8">No commands executed yet</p>
                        ) : (
                          commandHistory.map((cmd, i) => (
                            <div
                              key={i}
                              className="flex items-center gap-2 py-2 border-b border-slate-700/50 last:border-0 hover:bg-slate-700/30 rounded px-2 cursor-pointer"
                              onClick={() => {
                                setCommand(cmd);
                                setActiveTab('terminal');
                              }}
                            >
                              <span className="text-slate-500 font-mono text-sm">{i + 1}</span>
                              <span className="text-purple-400 font-mono">{cmd}</span>
                            </div>
                          ))
                        )}
                      </div>
                    </div>
                  )}

                  {activeTab === 'logs' && (
                    <div className="space-y-4">
                      <div className="flex justify-between items-center mb-4">
                        <h2 className="text-xl font-bold text-white">Access Logs</h2>
                        <button
                          onClick={loadLogs}
                          className="flex items-center gap-2 px-4 py-2 bg-purple-500/20 text-purple-300 rounded-lg hover:bg-purple-500/30"
                        >
                          <div className={`w-4 h-4 ${loading ? 'animate-spin' : ''}`}><RefreshCw /></div>
                          <span>Refresh</span>
                        </button>
                      </div>
                      <div className="bg-slate-900/80 rounded-lg p-4 max-h-96 overflow-y-auto">
                        <pre className="text-slate-300 text-xs font-mono whitespace-pre-wrap">
                          {logs || 'No logs available'}
                        </pre>
                      </div>
                    </div>
                  )}
                </div>
              </div>
            </div>
          );
        };

        ReactDOM.render(<RemoteDesktopControl />, document.getElementById('root'));

    </script>
</body>
</html>
)HTML";
}