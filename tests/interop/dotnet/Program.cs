// Interop check: OPC Foundation UA-.NETStandard reference client -> muc-opcua server.
//
// This is the authoritative cross-stack interop signal (the .NET reference stack is the
// implementation the OPC Foundation UACTT is built on). It is the server-bound leg of the
// async-opcua interop harness adapted for muc-opcua, which is server-only.
//
// Connects with SecurityPolicy Basic256Sha256 / SignAndEncrypt + Anonymous (selecting the
// most-secure endpoint) and validates the Nano-profile surface: connect
// (HEL/OPN/GetEndpoints/CreateSession/ActivateSession), Read, Browse, mandatory attribute
// reads, and a negative (unknown node) case. Exits non-zero if any check fails.

using System;
using System.Threading.Tasks;
using Opc.Ua;
using Opc.Ua.Client;
using Opc.Ua.Configuration;

namespace MucOpcUa.Interop
{
    internal static class Program
    {
        private static int _failures;

        private static void Check(string name, bool ok, string detail = "")
        {
            Console.WriteLine($"  [{(ok ? "PASS" : "FAIL")}] {name}{(detail.Length > 0 ? " — " + detail : "")}");
            if (!ok) _failures++;
        }

        private static async Task<int> Main(string[] args)
        {
            string url = args.Length > 0 ? args[0] : "opc.tcp://localhost:4840";

            var config = new ApplicationConfiguration
            {
                ApplicationName = "muc-opcua-interop-client",
                ApplicationUri = "urn:muc-opcua:interop:client",
                ApplicationType = ApplicationType.Client,
                SecurityConfiguration = new SecurityConfiguration
                {
                    ApplicationCertificate = new CertificateIdentifier
                    {
                        StoreType = CertificateStoreType.Directory,
                        StorePath = "pki/own",
                        SubjectName = "CN=muc-opcua-interop-client"
                    },
                    TrustedIssuerCertificates = new CertificateTrustList { StoreType = CertificateStoreType.Directory, StorePath = "pki/issuer" },
                    TrustedPeerCertificates = new CertificateTrustList { StoreType = CertificateStoreType.Directory, StorePath = "pki/trusted" },
                    RejectedCertificateStore = new CertificateStoreIdentifier { StoreType = CertificateStoreType.Directory, StorePath = "pki/rejected" },
                    AutoAcceptUntrustedCertificates = true,
                    AddAppCertToTrustedStore = true,
                    MinimumCertificateKeySize = 2048
                },
                TransportConfigurations = new TransportConfigurationCollection(),
                TransportQuotas = new TransportQuotas { OperationTimeout = 15000 },
                ClientConfiguration = new ClientConfiguration { DefaultSessionTimeout = 60000 },
                TraceConfiguration = new TraceConfiguration()
            };

            await config.Validate(ApplicationType.Client).ConfigureAwait(false);
            config.CertificateValidator.CertificateValidation += (s, e) => { e.Accept = true; };

            var app = new ApplicationInstance { ApplicationConfiguration = config };
            await app.CheckApplicationInstanceCertificatesAsync(false).ConfigureAwait(false);

            Console.WriteLine($"Connecting to {url} with the OPC Foundation .NET reference client ...");
            // useSecurity: true -> pick the most-secure endpoint (Basic256Sha256 / SignAndEncrypt).
            var selected = CoreClientUtils.SelectEndpoint(config, url, true, 15000);
            var endpoint = new ConfiguredEndpoint(null, selected, EndpointConfiguration.Create(config));

            using var session = await Session.Create(
                config, endpoint, false, "muc-opcua interop", 60000,
                new UserIdentity(new AnonymousIdentityToken()), null).ConfigureAwait(false);

            Check("connect (HEL/OPN/GetEndpoints/CreateSession/ActivateSession)", session.Connected);
            Check("endpoint SecurityPolicy is Basic256Sha256 or Aes128_Sha256_RsaOaep",
                  selected.SecurityPolicyUri == SecurityPolicies.Basic256Sha256 ||
                  selected.SecurityPolicyUri == SecurityPolicies.Aes128_Sha256_RsaOaep, selected.SecurityPolicyUri);
            Check("endpoint SecurityMode is SignAndEncrypt",
                  selected.SecurityMode == MessageSecurityMode.SignAndEncrypt, selected.SecurityMode.ToString());

            // Read MyVar1 Value (Int32 = 42)
            var value = session.ReadValue(new NodeId(1000, 1));
            Check("read ns=1;i=1000 Value == 42",
                  StatusCode.IsGood(value.StatusCode) && Convert.ToInt32(value.Value) == 42,
                  $"status={value.StatusCode}, value={value.Value}");

            // Mandatory readable attributes: BrowseName (QualifiedName), DisplayName (LocalizedText), NodeClass.
            var toRead = new ReadValueIdCollection
            {
                new ReadValueId { NodeId = new NodeId(1000, 1), AttributeId = Attributes.BrowseName },
                new ReadValueId { NodeId = new NodeId(1000, 1), AttributeId = Attributes.DisplayName },
                new ReadValueId { NodeId = new NodeId(1000, 1), AttributeId = Attributes.NodeClass },
            };
            session.Read(null, 0, TimestampsToReturn.Neither, toRead, out var results, out _);
            var browseName = results[0].GetValue<QualifiedName>(null);
            var displayName = results[1].GetValue<LocalizedText>(null);
            Check("read BrowseName == MyVar1", browseName != null && browseName.Name == "MyVar1", browseName?.Name);
            Check("read DisplayName == MyVar1", displayName != null && displayName.Text == "MyVar1", displayName?.Text);
            Check("read NodeClass == Variable",
                  StatusCode.IsGood(results[2].StatusCode) && Convert.ToInt32(results[2].Value) == (int)NodeClass.Variable,
                  results[2].Value?.ToString());

            // Browse Objects for MyVar1 (HierarchicalReferences + includeSubtypes).
            session.Browse(null, null, new NodeId(85u), 0u, BrowseDirection.Forward,
                ReferenceTypeIds.HierarchicalReferences, true,
                (uint)(NodeClass.Object | NodeClass.Variable),
                out _, out var refs);
            bool found = false;
            foreach (var r in refs)
            {
                if (r.NodeId.ToString().Contains("i=1000")) found = true;
            }
            Check("browse Objects (i=85) -> MyVar1", found, $"{refs.Count} reference(s)");

            // Negative: an unknown node yields an operation-level Bad status.
            var badRead = new ReadValueIdCollection
            {
                new ReadValueId { NodeId = new NodeId(9999, 1), AttributeId = Attributes.Value },
            };
            session.Read(null, 0, TimestampsToReturn.Neither, badRead, out var badResults, out _);
            Check("read unknown node -> Bad status", StatusCode.IsBad(badResults[0].StatusCode), badResults[0].StatusCode.ToString());

            await session.CloseAsync().ConfigureAwait(false);

            Console.WriteLine(_failures == 0 ? "INTEROP PASS" : $"INTEROP FAIL ({_failures} check(s) failed)");
            return _failures == 0 ? 0 : 1;
        }
    }
}
